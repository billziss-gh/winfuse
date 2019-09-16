<h1 align="center">
    WinFuse &middot; FUSE for Windows and WSL
</h1>

<p align="center">
    <b>THIS PROJECT IS WORK IN PROGRESS</b>
    <br/>
    <br/>
    <a href="https://ci.appveyor.com/project/billziss-gh/winfuse">
        <img src="https://img.shields.io/appveyor/ci/billziss-gh/winfuse.svg"/>
    </a>
</p>

## Architecture

### Structural Diagram

The following component diagram shows the WinFuse project components and their relations to the Windows OS (NTOS), WSL (LXCORE) and components that are provided by other projects.

![Component Diagram](doc/component.svg)

The components and their interfaces are:

- The **winfsp** component which is provided by the [WinFsp](https://github.com/billziss-gh/winfsp) project.
    - The component is an NTOS driver.
    - The component provides an **FSP_FSCTL_TRANSACT** `DeviceIoControl` interface.
    - The component is responsible for loading the **winfuse** component.
- The **winfuse** component which is provided by the WinFuse project (this project).
    - The component is an NTOS driver.
    - The component provides an **FUSE_FSCTL_TRANSACT** `DeviceIoControl` interface.
    - The component connects to the **FSP_FSCTL_TRANSACT** interface. WinFsp file system operations retrieved via this interface are translated to FUSE file system operations and become available via **FUSE_FSCTL_TRANSACT**.
- The **wslfuse** component which is provided by the WinFuse project (this project).
    - The component is an LXCORE driver.
    - The component provides a **/dev/fuse** Linux interface.
    - The component connects to the **FUSE_FSCTL_TRANSACT** interface. FUSE file system operations retrieved via this interface become available via **/dev/fuse**.
- The **lxldr** component which is provided by the [LxDK](https://github.com/billziss-gh/lxdk) project.
    - The component is an LXCORE driver.
    - The component is responsible for loading the **wslfuse** component.
- The file system (**FS**) components which are user-mode programs provided by third parties.
    - A Windows WinFsp file system connects to the **FSP_FSCTL_TRANSACT** interface.
    - A Windows FUSE file system connects to the **FUSE_FSCTL_TRANSACT** interface.
    - A Linux FUSE file system connects to the **/dev/fuse** interface.

### Windows FUSE File System Behavioral Diagram

![Windows FS Sequence Diagram](doc/winseq.svg)

### Linux FUSE File System Behavioral Diagram

![Linux FS Sequence Diagram](doc/wslseq.svg)
