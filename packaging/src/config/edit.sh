#!/usr/bin/env bash
# config/edit.sh - Edit PiTrac configuration

# Get the profile to edit (basic or advanced)
profile="${args[--profile]:-basic}"
editor="${args[--editor]:-${EDITOR:-nano}}"

# Determine which config file to edit
if [[ "$profile" == "basic" ]]; then
    template="/etc/pitrac/config/settings-basic.yaml"
    description="basic settings"
elif [[ "$profile" == "advanced" ]]; then
    template="/etc/pitrac/config/settings-advanced.yaml"
    description="advanced settings"
else
    error "Unknown profile: $profile (use 'basic' or 'advanced')"
    exit 1
fi

# User config location
user_config_dir="${HOME}/.pitrac/config"
user_config="${user_config_dir}/pitrac.yaml"

# Create user config directory if it doesn't exist
if [[ ! -d "$user_config_dir" ]]; then
    info "Creating user configuration directory: $user_config_dir"
    mkdir -p "$user_config_dir"
fi

# If user config doesn't exist, copy from template
if [[ ! -f "$user_config" ]]; then
    if [[ -f "$template" ]]; then
        info "Creating user configuration from $description template"
        cp "$template" "$user_config"
    else
        # Fallback to creating minimal config
        cat > "$user_config" << 'EOF'
# PiTrac User Configuration
# This file overrides settings in /etc/pitrac/config/
# Edit with: pitrac config edit
# Validate with: pitrac config validate

version: 2.0
profile: basic

# Add your configuration overrides below
# See /etc/pitrac/config/settings-basic.yaml for available options

EOF
    fi
fi

# Check if we need to upgrade to advanced profile
if [[ "$profile" == "advanced" ]]; then
    current_profile=$(grep "^profile:" "$user_config" | awk '{print $2}')
    if [[ "$current_profile" == "basic" ]]; then
        info "Upgrading configuration to advanced profile"
        
        # Backup current config
        backup="${user_config}.backup.$(date +%Y%m%d_%H%M%S)"
        cp "$user_config" "$backup"
        log_debug "Backed up current config to: $backup"
        
        # Update profile
        sed -i "s/^profile: basic/profile: advanced/" "$user_config"
        
        # Append advanced settings section if template exists
        if [[ -f "$template" ]]; then
            echo "" >> "$user_config"
            echo "# ============================================================================" >> "$user_config"
            echo "# ADVANCED SETTINGS" >> "$user_config"
            echo "# ============================================================================" >> "$user_config"
            echo "" >> "$user_config"
            
            # Extract advanced sections from template
            awk '/^ball_detection_advanced:/,/^[^[:space:]]/ { if (!/^[^[:space:]]/ || /^ball_detection_advanced:/) print }' "$template" >> "$user_config"
            awk '/^ai_detection:/,/^[^[:space:]]/ { if (!/^[^[:space:]]/ || /^ai_detection:/) print }' "$template" >> "$user_config"
            awk '/^camera_advanced:/,/^[^[:space:]]/ { if (!/^[^[:space:]]/ || /^camera_advanced:/) print }' "$template" >> "$user_config"
        fi
    fi
fi

# Open editor
info "Opening $profile configuration in $editor"
info "Configuration file: $user_config"
"$editor" "$user_config"

# Validate after editing
if command -v yq >/dev/null 2>&1; then
    if yq eval '.' "$user_config" >/dev/null 2>&1; then
        success "Configuration syntax is valid"
        info "Run 'pitrac config validate' to check values"
    else
        error "Configuration has syntax errors!"
        error "Please fix the errors and try again"
        exit 1
    fi
elif command -v python3 >/dev/null 2>&1; then
    if python3 -c "import yaml; yaml.safe_load(open('$user_config'))" 2>/dev/null; then
        success "Configuration syntax is valid"
        info "Run 'pitrac config validate' to check values"
    else
        error "Configuration has syntax errors!"
        error "Please fix the errors and try again"
        exit 1
    fi
else
    warn "Cannot validate YAML syntax (install yq or python3-yaml)"
    info "Run 'pitrac config validate' to check configuration"
fi