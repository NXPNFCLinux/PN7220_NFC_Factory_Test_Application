Application that helps user to controll PN7220 directly from driver.
Clone the repo into android_build/vendor/nfc
Build the application
Push it to device:
    adb push NfcFactoryTestApp data/
    chmod +x NfcFactoryTestApp
    ./NfcFactoryTestApp

Initial version supports
  Switch ModeSwitch GPIO to EMVCo mode
  Switch ModeSwitch GPIO to NFC Forum mode
  Standby mode enable
