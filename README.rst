============================
Facebook OpenBMC QEMU README
============================

Build QEMU first:

.. code-block:: shell

  git clone https://github.com/facebook/openbmc-qemu
  cd qemu
  ./configure --target-list=arm-softmmu
  make -j $(nproc)

Check the list of machine types:

.. code-block:: shell

  ./build/qemu-system-arm -machine help | grep Facebook
  # angelslanding-bmc    Facebook Angels Landing BMC (ARM1176)
  # brycecanyon-bmc      Facebook BryceCanyon BMC (ARM1176)
  # clearcreek-bmc       Facebook Clear Creek BMC (ARM1176)
  # cloudripper-bmc      Facebook Cloudripper BMC (Cortex-A7)
  # cmm-bmc              Facebook Backpack CMM BMC (ARM1176)
  # elbert-bmc           Facebook Elbert BMC (Cortex-A7)
  # elbertvboot-bmc      Facebook Elbert BMC (Cortex-A7)
  # emeraldpools-bmc     Facebook Emerald Pools BMC (ARM1176)
  # fby35-bmc            Facebook fby35 BMC (Cortex-A7)
  # fuji-bmc             Facebook Fuji BMC (Cortex-A7)
  # galaxy100-bmc        Facebook Galaxy 100 BMC (ARM926EJ-S)
  # grandcanyon-bmc      Facebook GrandCanyon BMC (Cortex-A7)
  # minipack-bmc         Facebook Minipack 100 BMC (ARM1176)
  # northdome-bmc        Facebook Northdome BMC (ARM1176)
  # tiogapass-bmc        Facebook TiogaPass BMC (ARM1176)
  # wedge100-bmc         Facebook Wedge 100 BMC (ARM926EJ-S)
  # wedge400-bmc         Facebook Wedge 400 BMC (ARM1176)
  # yamp-bmc             Facebook YAMP 100 BMC (ARM1176)
  # yosemitev2-bmc       Facebook Yosemitev2 BMC (ARM1176)
  # yosemitev3-bmc       Facebook Yosemitev3 BMC (ARM1176)

See https://github.com/facebook/openbmc to build an OpenBMC flash image.

Create a fixed-size MTD image from the OpenBMC flash image. 128MB is the
larget size, so it works for all platforms.

.. code-block:: shell

  # For example, after building flash-fby2 (YosemiteV2)
  dd if=/dev/zero of=fby2.mtd bs=1M count=128
  dd if=flash-fby2 of=fby2.mtd bs=1k conv=notrunc

To boot from the image:

.. code-block:: shell

  ./build/qemu-system-arm -machine yosemitev2-bmc \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -serial stdio -display none \

Note: 2 drive arguments are provided, first is primary flash (recovery
image) second is secondary flash (normal image).

To add port forwarding from the host port 2222 to the guest port 22:

.. code-block:: shell

  ./build/qemu-system-arm -machine yosemitev2-bmc \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -serial stdio -display none \
    -nic user,hostfwd=::2222-:22

And then you can ssh in:

.. code-block:: shell

  sshpass -p 0penBmc ssh root@localhost -p 2222

TUN/TAP configuration:

.. code-block:: shell

  sudo ip link add dev bmc-br0 type bridge
  sudo ip link set dev bmc-br0 up
  sudo ip tuntap add tap0 mode tap
  sudo ip link set tap0 up
  sudo brctl addif bmc-br0 tap0

  ./build/qemu-system-arm -machine yosemitev2-bmc \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -drive file=fby2.mtd,format=raw,if=mtd \
    -serial stdio -display none \
    -netdev tap,id=tap0,script=no,ifname=tap0 \
    -net nic,netdev=tap0,macaddr=00:11:22:33:44:55,model=ftgmac100
  # ... after boot, find link-local IPv6 address in the guest OS
  ip address show dev eth0 scope link
  # 2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
  #     link/ether 8e:0c:2d:76:b8:6b brd ff:ff:ff:ff:ff:ff
  #    inet6 fe80::8c0c:2dff:fe76:b86b/64 scope link
  #        valid_lft forever preferred_lft forever

  # Then you can ping or ssh to from the host to the guest
  ping6 fe80::8c0c:2dff:fe76:b86b%bmc-br0
  sshpass -p 0penBmc ssh root@fe80::8c0c:2dff:fe76:b86b%bmc-br0

===========
QEMU README
===========

QEMU is a generic and open source machine & userspace emulator and
virtualizer.

QEMU is capable of emulating a complete machine in software without any
need for hardware virtualization support. By using dynamic translation,
it achieves very good performance. QEMU can also integrate with the Xen
and KVM hypervisors to provide emulated hardware while allowing the
hypervisor to manage the CPU. With hypervisor support, QEMU can achieve
near native performance for CPUs. When QEMU emulates CPUs directly it is
capable of running operating systems made for one machine (e.g. an ARMv7
board) on a different machine (e.g. an x86_64 PC board).

QEMU is also capable of providing userspace API virtualization for Linux
and BSD kernel interfaces. This allows binaries compiled against one
architecture ABI (e.g. the Linux PPC64 ABI) to be run on a host using a
different architecture ABI (e.g. the Linux x86_64 ABI). This does not
involve any hardware emulation, simply CPU and syscall emulation.

QEMU aims to fit into a variety of use cases. It can be invoked directly
by users wishing to have full control over its behaviour and settings.
It also aims to facilitate integration into higher level management
layers, by providing a stable command line interface and monitor API.
It is commonly invoked indirectly via the libvirt library when using
open source applications such as oVirt, OpenStack and virt-manager.

QEMU as a whole is released under the GNU General Public License,
version 2. For full licensing details, consult the LICENSE file.


Documentation
=============

Documentation can be found hosted online at
`<https://www.qemu.org/documentation/>`_. The documentation for the
current development version that is available at
`<https://www.qemu.org/docs/master/>`_ is generated from the ``docs/``
folder in the source tree, and is built by `Sphinx
<https://www.sphinx-doc.org/en/master/>_`.


Building
========

QEMU is multi-platform software intended to be buildable on all modern
Linux platforms, OS-X, Win32 (via the Mingw64 toolchain) and a variety
of other UNIX targets. The simple steps to build QEMU are:


.. code-block:: shell

  mkdir build
  cd build
  ../configure
  make

Additional information can also be found online via the QEMU website:

* `<https://wiki.qemu.org/Hosts/Linux>`_
* `<https://wiki.qemu.org/Hosts/Mac>`_
* `<https://wiki.qemu.org/Hosts/W32>`_


Submitting patches
==================

The QEMU source code is maintained under the GIT version control system.

.. code-block:: shell

   git clone https://gitlab.com/qemu-project/qemu.git

When submitting patches, one common approach is to use 'git
format-patch' and/or 'git send-email' to format & send the mail to the
qemu-devel@nongnu.org mailing list. All patches submitted must contain
a 'Signed-off-by' line from the author. Patches should follow the
guidelines set out in the `style section
<https://www.qemu.org/docs/master/devel/style.html>` of
the Developers Guide.

Additional information on submitting patches can be found online via
the QEMU website

* `<https://wiki.qemu.org/Contribute/SubmitAPatch>`_
* `<https://wiki.qemu.org/Contribute/TrivialPatches>`_

The QEMU website is also maintained under source control.

.. code-block:: shell

  git clone https://gitlab.com/qemu-project/qemu-web.git

* `<https://www.qemu.org/2017/02/04/the-new-qemu-website-is-up/>`_

A 'git-publish' utility was created to make above process less
cumbersome, and is highly recommended for making regular contributions,
or even just for sending consecutive patch series revisions. It also
requires a working 'git send-email' setup, and by default doesn't
automate everything, so you may want to go through the above steps
manually for once.

For installation instructions, please go to

*  `<https://github.com/stefanha/git-publish>`_

The workflow with 'git-publish' is:

.. code-block:: shell

  $ git checkout master -b my-feature
  $ # work on new commits, add your 'Signed-off-by' lines to each
  $ git publish

Your patch series will be sent and tagged as my-feature-v1 if you need to refer
back to it in the future.

Sending v2:

.. code-block:: shell

  $ git checkout my-feature # same topic branch
  $ # making changes to the commits (using 'git rebase', for example)
  $ git publish

Your patch series will be sent with 'v2' tag in the subject and the git tip
will be tagged as my-feature-v2.

Bug reporting
=============

The QEMU project uses GitLab issues to track bugs. Bugs
found when running code built from QEMU git or upstream released sources
should be reported via:

* `<https://gitlab.com/qemu-project/qemu/-/issues>`_

If using QEMU via an operating system vendor pre-built binary package, it
is preferable to report bugs to the vendor's own bug tracker first. If
the bug is also known to affect latest upstream code, it can also be
reported via GitLab.

For additional information on bug reporting consult:

* `<https://wiki.qemu.org/Contribute/ReportABug>`_


ChangeLog
=========

For version history and release notes, please visit
`<https://wiki.qemu.org/ChangeLog/>`_ or look at the git history for
more detailed information.


Contact
=======

The QEMU community can be contacted in a number of ways, with the two
main methods being email and IRC

* `<mailto:qemu-devel@nongnu.org>`_
* `<https://lists.nongnu.org/mailman/listinfo/qemu-devel>`_
* #qemu on irc.oftc.net

Information on additional methods of contacting the community can be
found online via the QEMU website:

* `<https://wiki.qemu.org/Contribute/StartHere>`_
