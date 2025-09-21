/** @type {import('tailwindcss').Config} */
module.exports = {
    content: [
        './templates/**/*.{html,js}',
        './static/js/**/*.js'
    ],
    darkMode: 'class',
    theme: {
        extend: {
            colors: {
                // Golf-specific color palette
                'golf-green': {
                    50: '#f0fdf4',
                    100: '#dcfce7',
                    200: '#bbf7d0',
                    300: '#86efac',
                    400: '#4ade80',
                    500: '#22c55e',
                    600: '#16a34a',
                    700: '#15803d',
                    800: '#166534',
                    900: '#14532d'
                },
                'fairway': {
                    light: '#7cb342',
                    DEFAULT: '#689f38',
                    dark: '#558b2f'
                },
                'rough': {
                    light: '#8d6e63',
                    DEFAULT: '#6d4c41',
                    dark: '#4e342e'
                }
            },
            fontFamily: {
                'display': ['Inter var', 'system-ui', 'sans-serif'],
                'body': ['Inter var', 'system-ui', 'sans-serif'],
                'mono': ['JetBrains Mono', 'monospace']
            },
            animation: {
                'fade-in': 'fadeIn 0.5s ease-in-out',
                'slide-up': 'slideUp 0.3s ease-out',
                'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
                'spin-slow': 'spin 3s linear infinite',
                'bounce-slow': 'bounce 2s infinite',
                'gradient': 'gradient 15s ease infinite'
            },
            keyframes: {
                fadeIn: {
                    '0%': { opacity: '0' },
                    '100%': { opacity: '1' }
                },
                slideUp: {
                    '0%': { transform: 'translateY(10px)', opacity: '0' },
                    '100%': { transform: 'translateY(0)', opacity: '1' }
                },
                gradient: {
                    '0%, 100%': { backgroundPosition: '0% 50%' },
                    '50%': { backgroundPosition: '100% 50%' }
                }
            },
            backgroundImage: {
                'gradient-radial': 'radial-gradient(var(--tw-gradient-stops))',
                'gradient-conic': 'conic-gradient(from 180deg at 50% 50%, var(--tw-gradient-stops))',
                'golf-pattern': 'url(\'/static/images/golf-pattern.svg\')'
            }
        }
    },
    plugins: [
        require('@tailwindcss/forms'),
        require('@tailwindcss/typography'),
        require('@tailwindcss/aspect-ratio')
    ]
};
