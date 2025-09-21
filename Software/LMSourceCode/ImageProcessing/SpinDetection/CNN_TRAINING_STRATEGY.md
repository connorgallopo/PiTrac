# CNN-Based Spin Detection Training Strategy

## Overview
This document outlines a self-supervised approach to train a CNN for golf ball spin detection using PiTrac's existing brute-force algorithm as the ground truth generator.

## The Problem
- Current brute-force 3D rotation matching is **accurate but extremely slow** (~4,200 3D projections per calculation)
- No labeled ground truth spin data exists (only images)
- Need 100-1000x speedup while maintaining accuracy

## The Solution: Self-Supervised Learning
Use the slow-but-accurate current algorithm to generate training data offline, then train a CNN to approximate it in real-time.

```
[Current Algorithm] → [Generate Labels Offline] → [Train CNN] → [Real-time Inference]
     (Slow)                  (One-time)              (Fast)         (Milliseconds)
```

## Architecture Choice: Regression CNN vs YOLO

### ❌ Why NOT YOLO
YOLO is designed for **object detection** (bounding boxes + classes), not regression:
- YOLO predicts: `[x, y, width, height, class_probability]`
- We need: `[x_rotation, y_rotation, z_rotation]` (continuous angles)
- YOLO's architecture is overkill for our single-ball, fixed-position scenario

### ✅ Recommended: Custom Regression CNN
A lightweight CNN specifically designed for rotation estimation:

```python
class SpinDetectionCNN(nn.Module):
    def __init__(self):
        super().__init__()
        # Dual-stream architecture for two ball images
        self.feature_extractor = nn.Sequential(
            # Shared weights for both images
            nn.Conv2d(1, 32, 3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Conv2d(32, 64, 3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Conv2d(64, 128, 3, padding=1),
            nn.ReLU(),
            nn.AdaptiveAvgPool2d((4, 4))
        )
        
        # Combine features from both images
        self.fusion = nn.Sequential(
            nn.Linear(128 * 4 * 4 * 2, 512),  # *2 for two images
            nn.ReLU(),
            nn.Dropout(0.5),
            nn.Linear(512, 256),
            nn.ReLU(),
            nn.Dropout(0.5),
            nn.Linear(256, 3)  # Output: [x_rot, y_rot, z_rot]
        )
    
    def forward(self, img1, img2):
        # Extract features from both images
        feat1 = self.feature_extractor(img1).flatten(1)
        feat2 = self.feature_extractor(img2).flatten(1)
        
        # Concatenate and predict rotation
        combined = torch.cat([feat1, feat2], dim=1)
        rotation = self.fusion(combined)
        return rotation  # [batch, 3] for x,y,z rotations in degrees
```

## Training Data Generation Pipeline

### Step 1: Offline Label Generation
```bash
# Run on powerful workstation, not Pi
./generate_training_labels --input-dir /ball_images \
                          --output-dir /training_data \
                          --num-workers 8
```

```cpp
// generate_training_labels.cpp
void GenerateTrainingData() {
    vector<BallImagePair> pairs = LoadAllBallPairs();
    
    #pragma omp parallel for num_threads(8)
    for (auto& pair : pairs) {
        // Use existing high-accuracy algorithm
        Vec3d rotation = BallImageProc::GetBallRotation(
            pair.img1, pair.ball1, 
            pair.img2, pair.ball2
        );
        
        // Save preprocessed images and labels
        SaveTrainingExample(
            ApplyGaborFilter(pair.img1),  // Already filtered
            ApplyGaborFilter(pair.img2),
            rotation
        );
    }
}
```

### Step 2: Data Augmentation
Increase dataset size and robustness:
```python
def augment_training_data(img1, img2, rotation_label):
    augmentations = []
    
    # Brightness/contrast variations
    for gamma in [0.8, 1.0, 1.2]:
        aug_img1 = adjust_gamma(img1, gamma)
        aug_img2 = adjust_gamma(img2, gamma)
        augmentations.append((aug_img1, aug_img2, rotation_label))
    
    # Small rotations (simulate camera alignment errors)
    for angle in [-2, 0, 2]:
        aug_img1 = rotate(img1, angle)
        aug_img2 = rotate(img2, angle)
        # Adjust z-rotation label accordingly
        adjusted_label = rotation_label.copy()
        adjusted_label[2] += angle
        augmentations.append((aug_img1, aug_img2, adjusted_label))
    
    # Noise injection
    for noise_level in [0.01, 0.02]:
        aug_img1 = add_gaussian_noise(img1, noise_level)
        aug_img2 = add_gaussian_noise(img2, noise_level)
        augmentations.append((aug_img1, aug_img2, rotation_label))
    
    return augmentations
```

### Step 3: Training Process
```python
import torch
import torch.nn as nn
from torch.utils.data import DataLoader

class SpinDataset(Dataset):
    def __init__(self, data_dir):
        self.samples = load_preprocessed_samples(data_dir)
    
    def __getitem__(self, idx):
        img1, img2, rotation = self.samples[idx]
        return {
            'img1': torch.tensor(img1).float() / 255.0,
            'img2': torch.tensor(img2).float() / 255.0,
            'rotation': torch.tensor(rotation).float()
        }

def train_spin_detector():
    model = SpinDetectionCNN()
    criterion = nn.MSELoss()  # Or SmoothL1Loss for robustness
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-4)
    
    # Custom loss that weighs angles differently
    def weighted_rotation_loss(pred, target):
        weights = torch.tensor([1.0, 1.0, 2.0])  # Z-rotation more important
        return torch.mean(weights * (pred - target) ** 2)
    
    for epoch in range(100):
        for batch in dataloader:
            pred_rotation = model(batch['img1'], batch['img2'])
            loss = weighted_rotation_loss(pred_rotation, batch['rotation'])
            
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
        
        # Validate on held-out set
        validate(model, val_loader)
```

## Deployment Strategy

### Step 1: Export to ONNX
```python
# Export trained PyTorch model to ONNX
dummy_img1 = torch.randn(1, 1, 200, 200)
dummy_img2 = torch.randn(1, 1, 200, 200)

torch.onnx.export(
    model,
    (dummy_img1, dummy_img2),
    "spin_detector.onnx",
    input_names=['ball_image_1', 'ball_image_2'],
    output_names=['rotation_xyz'],
    dynamic_axes={'ball_image_1': {0: 'batch'},
                  'ball_image_2': {0: 'batch'}}
)
```

### Step 2: Integrate with C++ Pipeline
```cpp
class CNNSpinDetector {
private:
    cv::dnn::Net net;
    
public:
    CNNSpinDetector() {
        net = cv::dnn::readNetFromONNX("spin_detector.onnx");
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    
    cv::Vec3d GetRotation(const cv::Mat& img1, const cv::Mat& img2) {
        // Preprocess (Gabor filter - same as training)
        cv::Mat filtered1 = ApplyGaborFilter(img1);
        cv::Mat filtered2 = ApplyGaborFilter(img2);
        
        // Prepare input blob
        cv::Mat blob1 = cv::dnn::blobFromImage(filtered1, 1.0/255.0);
        cv::Mat blob2 = cv::dnn::blobFromImage(filtered2, 1.0/255.0);
        
        // Run inference
        net.setInput(blob1, "ball_image_1");
        net.setInput(blob2, "ball_image_2");
        cv::Mat output = net.forward("rotation_xyz");
        
        // Extract rotation values
        return cv::Vec3d(
            output.at<float>(0, 0),  // x_rotation
            output.at<float>(0, 1),  // y_rotation  
            output.at<float>(0, 2)   // z_rotation
        );
    }
};
```

## Performance Expectations

### Training Requirements
- **Dataset Size**: 10,000-50,000 ball pairs
- **Generation Time**: ~1-2 seconds per pair (current algorithm)
- **Total Generation**: 3-14 hours on 8-core workstation
- **Training Time**: 2-4 hours on GPU

### Inference Performance
- **Current Algorithm**: 500-1000ms per spin calculation
- **CNN Inference**: 5-10ms on Pi 5 CPU
- **Speedup**: **100-200x**

### Accuracy Expectations
- **Initial Accuracy**: 85-90% within ±2 degrees
- **After Fine-tuning**: 95%+ within ±1 degree
- **Hybrid Approach**: 99%+ (CNN for coarse + limited brute force)

## Validation Strategy

### 1. Cross-Method Validation
```python
def validate_accuracy():
    test_set = load_test_images()
    
    for img_pair in test_set:
        # Ground truth from current algorithm
        true_rotation = brute_force_method(img_pair)
        
        # CNN prediction
        cnn_rotation = cnn_model(img_pair)
        
        # Alternative methods for validation
        feature_rotation = feature_matching_method(img_pair)
        phase_rotation = phase_correlation_method(img_pair)
        
        # Compare all methods
        print(f"True: {true_rotation}")
        print(f"CNN:  {cnn_rotation} (error: {np.abs(cnn_rotation - true_rotation)})")
        print(f"Feature: {feature_rotation}")
        print(f"Phase: {phase_rotation}")
```

### 2. Visual Validation
- Rotate ball 1 by predicted angles
- Compare visually with ball 2
- Should see dimple alignment

### 3. Confidence Scoring
Add uncertainty estimation to the CNN:
```python
class SpinDetectionCNNWithUncertainty(nn.Module):
    def forward(self, img1, img2):
        # ... feature extraction ...
        rotation = self.rotation_head(features)
        confidence = torch.sigmoid(self.confidence_head(features))
        return rotation, confidence  # High confidence = trust CNN
```

## Implementation Timeline

### Phase 1: Data Generation (Week 1)
- [ ] Set up parallel processing pipeline
- [ ] Generate 10,000 labeled examples
- [ ] Implement data augmentation

### Phase 2: Model Development (Week 2)
- [ ] Design CNN architecture
- [ ] Implement training pipeline
- [ ] Hyperparameter tuning

### Phase 3: Integration (Week 3)
- [ ] Export to ONNX
- [ ] C++ integration
- [ ] Performance benchmarking

### Phase 4: Validation (Week 4)
- [ ] Accuracy testing
- [ ] Edge case handling
- [ ] Production deployment

## Fallback Strategy

Implement confidence-based switching:
```cpp
cv::Vec3d GetRotationWithFallback(imgs...) {
    // Try CNN first (fast)
    auto [rotation, confidence] = CNNSpinDetector::GetRotation(imgs);
    
    if (confidence > 0.9) {
        return rotation;  // Trust CNN
    } else if (confidence > 0.7) {
        // Refine with limited search
        return RefineWithBruteForce(rotation, ±3_degrees);
    } else {
        // Fall back to full algorithm
        LogWarning("Low confidence, using slow algorithm");
        return BruteForceRotation(imgs);
    }
}
```

## Alternative: Lightweight Feature-Based CNN

Instead of predicting rotation directly, predict feature correspondences:
```python
class FeatureMatchingCNN(nn.Module):
    """Predicts matching dimple locations between frames"""
    def forward(self, img1, img2):
        # Output: correspondence map
        # Each pixel in img1 maps to location in img2
        return correspondence_map  # [H, W, 2] for (x,y) offsets
```

Then use geometric methods to compute rotation from correspondences.

## Conclusion

This self-supervised approach leverages your existing accurate (but slow) algorithm to train a fast CNN approximation. The key advantages:

1. **No manual labeling required**
2. **Maintains accuracy** of current system
3. **100-200x speedup** for real-time processing
4. **Fallback options** for low-confidence cases
5. **Continuous improvement** as you collect more data

The CNN learns to approximate the complex 3D rotation search in a single forward pass, making spin detection feasible on Pi 5 hardware.