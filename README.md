# Karton - The KDE Virtual Machine Manager

## DIRE WARNING, THIS IS NOT STABLE

If you're thinking about installing this, think again unless you know what
you're doing. Anything is subject to change without notice, including the VM
configuration file format. If you install this, update it, and suddenly all
your VMs vanish, don't say you weren't warned.

## Introduction

Karton is a desktop virtual machine management application for KDE. It is
designed to look nice and work well with a Plasma desktop session. It aims to
be an alternative to GNOME Boxes, VirtualBox, and virt-manager for desktop
virtualization use cases (though at the moment it is significantly simpler
than all of these). It is partially inspired by (though shares no code with)
the popular macOS virtualization app, [UTM](https://mac.getutm.app/). Under
the hood, it makes use of QEMU and KVM for virtualization and emulation.

Karton is currently under heavy development and is NOT feature-complete. While
all of the buttons work, there are many more features planned for in the
future. So if you use this right now and are thinking "man, this is way too
minimal, I wanted a virtual machine manager and not just a glorified QEMU
launcher", have patience, more things are coming.

## Installation

1. Read the dire warning at the top of this document. Proceed at your own
   risk.
2. Install prerequisites. You'll need Qt 6.2.0, the Config, CoreAddons, and
   Kirigami KDE frameworks (version 6.0.0 or higher), and
   `qemu-system-x86_64`. Karton is designed to run under Plasma 6.
3. Clone the Git repo to your local machine:

   `git clone https://invent.kde.org/arraybolt/karton.git`

4. Build and install it (ignore the CMake warnings, I haven't figured out why
   those are popping up yet):

   `cd karton && mkdir build && cd build && cmake .. && make -j$(nproc) && sudo make install`

5. Check your app menu for "Karton", it should be hiding under the "System"
   section.
6. Launch it and try making a VM!

Some notes:

* Launching a VM will pop up a QEMU window. This is intentional, and is a
  result of the fact that a proper VM console has not yet been developed. It's
  an intended feature which should be added in the near future.
* You will not be able to close this QEMU window by normal means (clicking the
  "X" button or pressing Alt+F4). This is because the developer has so many
  times forcibly terminated virtual machines that he has elected to inhibit
  normal window closing to save others this same pain. If you really, actually
  want to forcibly power off a VM, click "Machine -> Quit" in the VM window.
* The list of operating systems available in the create and configure windows
  is not representative of all the operating systems Karton can run. If you
  don't see your OS in the list, just choose "Other".
* Yes, the app is very small and doesn't have a lot of features. This is just
  the beginning though, expect frequent updates as things move forward.
* Unlike libvirt-based QEMU frontends, you *can* change the firmware of a VM
  after creating it by using the configure menu. You will *probably* break
  your VM if you attempt this stunt.

## Contributing

We have a development chat on Matrix at
[#karton:kde.org](https://matrix.to/#/#karton:kde.org). Feel free to swing by
and offer advice and feedback!

Please make feature requests. This thing is way too bare-bones as it is.

Be aware that due to Karton's state of rapid development, the code may change
in significant ways from one day to the next. If you want to make an MR, it's
probably best to coordinate with me first in our Matrix room linked above.

Hope you find Karton useful!
