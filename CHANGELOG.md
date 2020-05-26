# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added

- Add `SIM5320FTPClient::download` and `SIM5320FTPClient::upload` method version
  that accepts integer file descriptor.

### Changed

- Refactor `SIM5320FTPClient::download` and `SIM5320FTPClient::upload` method to use system calls
  `open`, `read`, `write`, `close` instead of `fopen`, `fread`, `fwrite`, `fclose` to reduce
  dynamic memory operations.

## [0.2.0] - 2020-05-17
### Fixed

- Fix gps rollover week issue (see [Declaration of GPS week rollover issue](https://cdn-learn.adafruit.com/assets/assets/000/084/848/original/GPS-week-rollover_Simcom.pdf?1574619386)).

### Changed

- Breakchange. Migrate from Mbed OS `5.15` to `6.0.0-beta-1` as framework implementation of the versions 5.15 and 5.14 contains
  a bug that causes errors when non-network API is used.
- Breakchange. Refactor `SIM5320GPSDevice` and rename it to `SIM5320LocationService`

### Added

- Added `TimeService` to synchronize time over network.
- Added support of the `cellular.use-sms` option.

## [0.1.2] - 2020-02-19

### Changed

- Update library for compatibility with Mbed OS 5.15.

## [0.1.1] - 2019-09-15

### Fixed

- Fixed compatibility with Mbed OS 5.13.4.
- Updated tests for a compatibility with open32f3-d extension board.

## [0.1.0] - 2019-05-01

### Added

- Implemented SMS API
- Implemented TCP/UPD API
- Implemented base Cellular API
- Added API for GPS
- Added API for FTP
