# Tiny OS

## Project Duration
Feb 2021 – Mar 2021

## Overview
A small operating system built on top of **Pintos**. Implemented the following key features:

### Features
- **Alarm Clock** – thread sleep/wake based on timer ticks  
- **Priority Scheduling** – schedule threads by priority  
- **Argument Passing** – pass command-line arguments to user programs  
- **User Memory Access**   
- **System Calls**  
- **Process Termination Message**

### Virtual Memory
- **Virtual Memory Management** (pages, page table)  
- **Lazy Loading** – initialize pages on-demand  
- **Page Cleanup** – reclaim resources on eviction/exit  
- **Anonymous Page**
- **Memory-Mapped Page** 
- **Swap In/Out**

### File System
- **Indexed & Extensible Files**  
- **Subdirectories** 
- **Symlink**
  
## Build & Test
- compile & build : type `source ./activate`.
- test : change directory to each folder(filesys, threads, userprog, vm) and type `make check`.
    - Individual testing: `pintos -- -q run {test_file}` (e.g., `alarm-multiple`). See the `tests` directory.
