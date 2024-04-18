# Embedded Linux

This repository contains a sample application for the Beaglebone Black, demonstrating the use of a Logitech C920 camera within an embedded Linux environment. The project highlights the integration of the camera with a fully functional Operating System Abstraction Layer (OSAL), which simulates RTOS-like behavior managing tasks, semaphores, and mutexes.

## Features

- **RTOS-like OSAL**: Implements an Operating System Abstraction Layer to manage RTOS-like primitives including tasks, semaphores, and mutexes.
- **Camera Control**: Utilizes the V4L2 library to control the Logitech C920 camera, implementing memory mapped buffers for efficient frame handling.

## Prerequisites

Before you begin, ensure you have met the following requirements:
- SoC with a Linux operating system.
- USB Camera compatible with V4L2.
- Linux kernel with V4L2 support.
- Linux kernel patched with `RT PREEMPT`

## Installation

To get started with this project, clone the repository and deploy it to your Beaglebone Black:

```bash
git clone https://github.com/bpowell461/Embedded-Linux.git
cd Embedded-Linux
```
