# Web UI Visual Design

## Color Scheme

**Background:** Linear gradient from purple (#667eea) to darker purple (#764ba2)

**Cards:** White with rounded corners, shadow, and slide-in animation

**Primary Color:** Purple (#667eea) - used for borders, buttons, badges
**Success:** Green (#10b981) - connected status, success messages
**Error:** Red (#ef4444) - error messages, danger buttons
**Info:** Blue (#3b82f6) - info messages, active badges
**Warning:** Yellow/Orange (#f59e0b) - inactive badges

## Layout Structure

```
┌────────────────────────────────────────────────────────────────┐
│  [Purple gradient background]                                  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 🌐 Network Manager                                       │  │
│  │                                                          │  │
│  │ [Gradient purple box with status info]                  │  │
│  │   Device:        NetworkManager                         │  │
│  │   IP Address:    192.168.1.100                         │  │
│  │   Status:        ✅ Connected (green)                   │  │
│  │   Current SSID:  HomeNetwork                            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 📡 Configured Networks                                   │  │
│  │                                                          │  │
│  │ ┌────────────────────────────────────────────────────┐  │  │
│  │ │ HomeNetwork 🔒                                     │  │  │
│  │ │ [CURRENT] [ACTIVE]                        iwcWifi0 │  │  │
│  │ │ [✓ Connected] button                               │  │  │
│  │ └────────────────────────────────────────────────────┘  │  │
│  │                                                          │  │
│  │ ┌────────────────────────────────────────────────────┐  │  │
│  │ │ OfficeNetwork 🔒                                   │  │  │
│  │ │ [ACTIVE]                                    wifi2  │  │  │
│  │ │ [📡 Connect] [🗑️ Delete] buttons                    │  │  │
│  │ └────────────────────────────────────────────────────┘  │  │
│  │                                                          │  │
│  │ ┌────────────────────────────────────────────────────┐  │  │
│  │ │ (empty)                                            │  │  │
│  │ │ [INACTIVE]                                  wifi3  │  │  │
│  │ │ [📡 Connect] [🗑️ Delete] buttons (disabled)         │  │  │
│  │ └────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ ➕ Add New Network                                       │  │
│  │                                                          │  │
│  │ Network Name (SSID) *                                   │  │
│  │ [___________________________________________]           │  │
│  │                                                          │  │
│  │ Password (leave empty for open networks)                │  │
│  │ [___________________________________________]           │  │
│  │                                                          │  │
│  │ [➕ Add Network] button                                  │  │
│  │                                                          │  │
│  │ [Success/error message appears here]                    │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ ⚙️ System Actions                                        │  │
│  │                                                          │  │
│  │ [🔄 Reconnect WiFi]  [⚙️ Configure]  [🔴 Reboot]       │  │
│  │  (green button)      (gray button)    (red button)     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

## Network Card States

### Current Network (Connected)
```
┌────────────────────────────────────────────────────────┐
│ [Green gradient background]                            │
│                                                        │
│ HomeNetwork 🔒                                         │
│ [CURRENT-green] [ACTIVE-blue]               iwcWifi0  │
│                                                        │
│ [✓ Connected - green button]                          │
└────────────────────────────────────────────────────────┘
```

### Active Network (Available)
```
┌────────────────────────────────────────────────────────┐
│ [Light gray background]                                │
│                                                        │
│ OfficeNetwork 🔒                                       │
│ [ACTIVE-blue]                                  wifi2  │
│                                                        │
│ [📡 Connect] [🗑️ Delete]                               │
│  (blue)       (red)                                    │
└────────────────────────────────────────────────────────┘
```

### Inactive Network (Empty)
```
┌────────────────────────────────────────────────────────┐
│ [Light gray background]                                │
│                                                        │
│ (empty)                                                │
│ [INACTIVE-yellow]                              wifi3  │
│                                                        │
│ [📡 Connect] [🗑️ Delete]                               │
│  (disabled)   (enabled)                                │
└────────────────────────────────────────────────────────┘
```

## Message States

### Success Message
```
┌────────────────────────────────────────────────────────┐
│ [Green background with darker green border-left]       │
│ ✅ Network added to slot 2! Status: connecting         │
└────────────────────────────────────────────────────────┘
```

### Error Message
```
┌────────────────────────────────────────────────────────┐
│ [Red background with darker red border-left]           │
│ ❌ Error: Network not found                            │
└────────────────────────────────────────────────────────┘
```

### Info Message
```
┌────────────────────────────────────────────────────────┐
│ [Blue background with darker blue border-left]         │
│ ⏳ Adding network...                                    │
└────────────────────────────────────────────────────────┘
```

## Responsive Mobile View

On mobile (< 600px):
- Cards stack vertically
- Buttons become full-width
- Network actions stack vertically
- System actions stack vertically
- Padding reduced for smaller screens
- All interactive elements remain usable
- Text remains readable

## Animations

1. **Card slide-in:** Cards slide up and fade in when page loads (0.3s)
2. **Button hover:** Buttons lift up slightly and show shadow on hover
3. **Message slide-in:** Messages slide in when shown (0.3s)
4. **Auto-hide:** Messages fade out after 8 seconds

## Typography

- **Headers:** -apple-system, BlinkMacSystemFont, 'Segoe UI', etc.
- **H1:** 28px, bold, with icon
- **H2:** 20px, bold, with bottom border
- **Body:** 14px, regular
- **Labels:** 14px, semi-bold
- **Badge text:** 11px, uppercase, bold

## Accessibility

- High contrast ratios for readability
- Clear visual hierarchy
- Large touch targets (44px minimum)
- Descriptive button labels with icons
- Form labels clearly associated with inputs
- Error messages are descriptive
- Confirmation dialogs for destructive actions

## Design Philosophy

**Modern, clean, and professional** - Not too playful, not too corporate.
Perfect for IoT device management interface that users will interact
with occasionally but need to trust and understand immediately.

The purple gradient gives it a tech feel, while the clean white cards
keep it approachable and easy to scan. Color-coding (green=good,
red=danger, blue=info) helps users quickly understand status at a glance.
