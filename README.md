# RAVENNA Software Development Kit (SDK)

## Introduction

This software library provides a set of tools and libraries to develop applications for RAVENNA Audio over IP
technology. It is based on the [RAVENNA Audio over IP technology](https://ravenna-network.com/technology/).

## RFCs

RFC 3550: RTP: A Transport Protocol for Real-Time Applications  
https://datatracker.ietf.org/doc/html/rfc3550

RFC 3551: RTP Profile for Audio and Video Conferences with Minimal Control  
https://datatracker.ietf.org/doc/html/rfc3551

RFC 3190: RTP Payload Format for 12-bit DAT Audio and 20- and 24-bit Linear Sampled Audio
https://datatracker.ietf.org/doc/html/rfc3190

RFC 8866: SDP: Session Description Protocol
https://datatracker.ietf.org/doc/html/rfc8866

RCC 4570: Session Description Protocol (SDP) Source Filters
https://datatracker.ietf.org/doc/html/rfc4570

## CMake options

| CMake option                    | Description                                          |
|---------------------------------|------------------------------------------------------|
| -DWITH_ADDRESS_SANITIZER=ON/OFF | When enabled, the address sanitizer will be engaged. |
| -DWITH_THREAD_SANITIZER=ON/OFF  | When enabled, the thread sanitizer will be engaged.  |

## Build and CMake options

| Compile option    | CMake option               | Description                                                                             |
|-------------------|----------------------------|-----------------------------------------------------------------------------------------|
| RAV_ENABLE_SPDLOG | -DRAV_ENABLE_SPDLOG=ON/OFF | When enabled, spdlog will be used for logging otherwise logs will be written to stdout. |
| TRACY_ENABLE      | -DTRACY_ENABLE=ON/OFF      | When enabled, Tracy will be compiled into the library.                                  |

## Quick commands

### Send audio as RTP stream example command

    ffmpeg -re -stream_loop -1 -f s16le -ar 48000 -ac 2 -i Sin420Hz@0dB16bit48kHzS.wav -c:a pcm_s16be -f rtp -payload_type 10 rtp://127.0.0.1:5004
