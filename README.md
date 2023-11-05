# rfid-race-timer

A file `secrets.h` is required, declaring the following constants:

 - `const String API_KEY` - the API key, as expected and declared by [the backend](https://github.com/Gatley-Run-Chip-Timing/backend).
 - `const String SSID` - the SSID to search for in order to access WiFi (2.4GHz only).
 - `const String PASSWORD` - the corresponding password for the network.
 - `const String ADD_TIME_CAPTURE_URL` - the hosted URL for the Firebase Function (or equivalent) implementation of `addTimeCaptureURL`, as in the corresponding [backend](https://github.com/Gatley-Run-Chip-Timing/backend). For the 2023 production implementation, this is `https://addrawtimecapture-iy6ymwjdha-ew.a.run.app`.
