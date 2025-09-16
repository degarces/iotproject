ESP32-CAM Telegram Motion Camera (AI-Thinker)

Motion-triggered ESP32-CAM that sends photos to a Telegram chat and supports bot commands (start/stop, flash, manual photo, light toggle). Built for the AI-Thinker module with PSRAM.

Security note: Never commit real bot tokens or Wi-Fi passwords. Revoke any tokens you’ve shared publicly and change your Wi-Fi password before pushing.

PIR-style motion detection with noise filtering (debounce + consistency threshold).
Auto-capture and send photo to Telegram on motion.

Telegram bot commands:
/start, /stop – enable/disable the system
/flashon, /flashoff – flash for pictures
/takephoto – snap on demand
/light – toggle flash LED as a torch
/help – list commands
Optional HTTP camera server (see note on startCameraServer()).
