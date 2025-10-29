# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed

- Fixed a crash caused by an assertion when receiving PTP management messages (and when `RAV_ABORT_ON_ASSERT` was
  enabled). Now PTP management and signalling messages are ignored.

## [v0.17.0] - October 27, 2025

### Fixed

- Removed some realtime unsafe branches from realtime paths.

## [v0.16.0] - October 16, 2025

### Changed

- Minor improvements and fixes to support sample rate conversion and drift correction.
- The pipeline will no longer trigger on pushes to the main branch.

## [v0.15.0] - September 8, 2025

### Added

- Added the RAV_DEBUG macro which enables certain debugging facilities. Can also be enabled for release builds.
- Added the RAV_ENABLE_DEBUG cmake option to enable RAV_DEBUG in release builds.

### Changed

- Renamed logging macros from RAV_LEVEL to RAV_LOG_LEVEL format.

### Fixed

- Improved streaming performance.

## [v0.14.0] - September 7, 2025

### Added

- The ability to configure the PTP domain.
- Added a callback rav::ptp::Instance::Subscriber::ptp_stats_updated to receive updates with PTP stats.

### Changed

- Methods of rav::ptp::Instance::Subscriber will now always be called when subscribing, regardless of the PTP state.

### Fixed

- Fixed an issue where RTP receiver state could be bouncing between receiving and ok_no_consumer.

## v0.13.1 - September 19, 2025
