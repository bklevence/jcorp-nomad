# <div align="center">Jcorp Nomad USB C Variant</div>

<div align="center">
  <img src="ESP32-S3-LCD-1.47B-details-interUSBC.jpg" alt="Jcorp Nomad Offline Media Server" width="800">
</div>

<p align="center"><b>A portable, offline media server powered by the ESP32-S3 in a thumbdrive form factor.</b><br>
Stream movies, music, books, and shows anywhere — no internet required.</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-CC--BY--NC--SA%204.0-blue.svg" alt="License: CC BY-NC-SA 4.0" />
  <img src="https://img.shields.io/badge/platform-ESP32--S3-orange" alt="Platform: ESP32-S3" />
  <img src="https://img.shields.io/badge/status-beta-lightgrey" alt="Status: Beta" />
</p>

---

# Quick Notes

I had to change a pin in the [firmware/JcorpNomadProject/Display_ST7789.h](firmware/JcorpNomadProject/Display_ST7789.h) to get the screen working. Also, you need to have 
a proper cable and powersupply to get the screen operational. It may not work when powered from the USB on your computer. Personally I may find another 


## WIP Status

- [x] Get working on Waveshare ESP32-S3-LCD-1.47B (USB C)
    - [x] [https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47B](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47B)
- [ ] Learn how to change screen UI
- [ ] Update Trim down this bloated readme from original, it's quite messy.
    - [ ] Add clearer instructions for when I decide to do this again.
        - [ ] Troubleshooting should just be here, no need for [instructables](https://www.instructables.com/Jcorp-Nomad-Mini-WIFI-Media-Server/) .
        - [ ] quickstart lacking information (fat32 specs)
    - [x] remove updates/promos and condense to necessary information.   
    - [x] Add arduino libraries as directory rather than zip file in release...
- [ ] Possibly attempt to update libraries (if feasible) 
    - [ ] Double check on following since experimental merge:
         - [ ] "ArduinoJson" by Benoit Blanchon v7.3.0
         - [ ] "Async TCP" by ESP32Async v3.4.7
         - [ ] "ESP Async Webserver" by ESP32Async v3.7.1
         - [ ] "LVGL" by kisvegabor v8.4.0
- [ ] Large directories (80+ GB) can cause the admin file browser to crash when opened.  
- [ ] Some bugs remain with SD Card storage reading on both the Screen UI and the admin panel.

> [!IMPORTANT]
> The following is from the original repo, I will be coming back to edit at some point.


<!-- Existing README content continues below -->

## Quick Start

1. Flash the ESP32-S3 with the firmware in the `/firmware/` and utilize `/libraries/` directories using Arduino IDE.
  - Placeholder for proper Arduino settings for this board variant. 
2. Format your SD card as **FAT32**, Allocation size: 32K, name it whatever, and copy the files from `/SD_Card_Template/`.
3. Place your media files into the appropriate folders, [see structure below](#WIP-Status).
4. Insert the SD card and power the device.
5. Connect to the Wi-Fi network named `NomadServer`.
6. Your browser will be redirected to the offline media interface.
  - you may need to generate the media.json once you're in.

## Repo Structure

> [!NOTE]
> 0I need to clean this up a little, but linking things for now. 

- [/firmware](/firmware) – Literally just Arduino (see .ino file) firmware for ESP32-S3, I will probably spin this into the ardiuno folder as a sketch at some point, for now it's nice to have so I can see deltas between repos.
    - [/firmware/JcorpNomadProject/JcorpNomadProject.ino](/firmware/JcorpNomadProject/JcorpNomadProject.ino) - Wi-Fi name and password can be changed here
- [/SD_Card_Template](/SD_Card_Template/) – Web UI, HTML files, and [/SD_Card_Template/media.json](/SD_Card_Template/media.json). 
    - [/SD_Card_Template/menu.html](/SD_Card_Template/menu.html) - Branding (logo, favicon) can be replaced in here. Sections (Movies, Music, etc.) can be removed from here.
- [/Arduino/libraries/](/Arduino/libraries/) – arduino libraries I needed to get this off the ground. This was a zip as a release on main branch. 


## File Compatibility & Streaming

A compatibility guide for supported media types, streaming expectations, and additional notes on performance.

| Category      | Supported Formats                    | Notes                                                                                   |
|---------------|------------------------------------|-----------------------------------------------------------------------------------------|
| **Video**     | `.mp4`, `.mov`, `.mkv`, `.webm`   | Reliable playback for these formats. `.avi` and `.flv` are **not supported**. Encoding video files using **H.264** video codec and **AAC** audio codec tends to improve streaming performance and allow more concurrent streams.  1080P = 2-3 streams, lower quality = more streams.        |
| **Audio**     | `.mp3`, `.flac`, `.wav`             | Other audio formats are **not supported** by the built-in player.                       |
| **Books / Docs** | `.pdf` (recommended), `.epub` (download only) | `.pdf` files can be opened and read directly in the browser. `.epub` files are downloadable but cannot be viewed in the built-in reader. |
| **Images**     | `.jpg`             | Used for covers and folder images only, and ensure all images and media files use matching names for proper display.                       |


> [!IMPORTANT]
> By utilizing FAT32 storage we limit individual file sizes to under 4 GB.

## Example Folder Structure (on SD Card)

```
/Movies/
    Movie.mp4
    Movie.jpg
/Shows/
    Show Name/
        S01E01 - Episode 1 Title.mp4
        S01E02 - Episode 2 Title.mp4
    Show Name.jpg
/Books/
    Book Title.pdf
    Book Title.jpg
/Music/
    track01.mp3

index.html
appleindex.html
menu.html
movies.html
shows.html
books.html
music.html
media.json
placeholder.jpg
Logo.png
favicon.ico
```

 
