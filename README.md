My Tiny OS
---

## 프로젝트 기간
2021.03 ~ 2021.04

## 설명
- 나만의 OS 만들기(PintOS)
- 주요 기능 구현
    - Alarm Clock
    - Priority Scheduling
    - Argument Passing
    - User Memory Access
    - System Calls
    - Process Termination Message
    - Virtual Memory Management(Page, Page Table)
    - Page Initialization with Lazy Loading
    - Page Cleanup
    - Anonymous page
    - Memory-mapped page
    - Swap In/Out
    - Indexed and Extensible Files
    - Subdirectories
    - Symlink

## 빌드 & 테스트
- compile & build : type `source ./activate`.
- testing : change directory to each folder(filesys, threads, userprog, vm) and type `make check`.
    - 개별 테스팅 : `pintos -- -q run {test file}` (ex.alarm-multiple) / tests 폴더 참고