.. SPDX-License-Identifier: GPL-2.0+

Android A/B updates
===================

Overview
--------

A/B system updates ensures modern approach for system update. This feature
allows one to use two sets (or more) of partitions referred to as slots
(normally slot A and slot B). The system runs from the current slot while the
partitions in the unused slot can be updated [1]_.

A/B enablement
--------------

The A/B updates support can be activated by specifying next options in
your board configuration file::

    CONFIG_ANDROID_AB=y
    CONFIG_CMD_BCB=y

The disk space on target device must be partitioned in a way so that each
partition which needs to be updated has two or more instances. The name of
each instance must be formed by adding suffixes: ``_a``, ``_b``, ``_c``, etc.
For example: ``boot_a``, ``boot_b``, ``system_a``, ``system_b``, ``vendor_a``,
``vendor_b``.

As a result you can use ``bcb ab_select`` command to ensure A/B boot process in
your boot script. This command analyzes and processes A/B metadata stored on a
special partition (e.g. ``misc``) and determines which slot should be used for
booting up.

If the A/B metadata partition has a backup bootloader_message block that is used
to ensure one is always valid even in the event of interruption when writing, it
can be enabled in your board configuration file::

    CONFIG_ANDROID_AB_BACKUP_OFFSET=0x1000

Command usage
-------------

.. code-block:: none

    bcb ab_select <slot_var_name> <interface> <dev[:part_number|#part_name]>

for example::

    => bcb ab_select slot_name mmc 1:4

or::

    => bcb ab_select slot_name mmc 1#misc

Result::

    => env print slot_name
    slot_name=a

Based on this slot information, the current boot partition should be defined,
and next kernel command line parameters should be generated:

* ``androidboot.slot_suffix=``
* ``root=``

For example::

    androidboot.slot_suffix=_a root=/dev/mmcblk1p12

A/B metadata is organized according to AOSP reference [2]_. On the first system
start with A/B enabled, when ``misc`` partition doesn't contain required data,
the default A/B metadata will be created and written to ``misc`` partition.

Virtual A/B
-----------

With Virtual A/B [3]_ the updated slot is written as snapshots in
``userdata`` and merged into the ``super`` partition after the first
successful boot of the new slot. The boot control HAL records the merge
status in the system space of ``misc`` (``struct misc_virtual_ab_message``,
offset 32 KiB) and the bootloader has to respect it:

* while the status is ``MERGING``, ``ab_select_slot()`` boots the slot the
  merge was started from (``source_slot``) whatever the slot metadata says,
  and ``ab_set_active_slot()`` (fastboot ``set_active``) is refused;
* while it is ``SNAPSHOTTED`` or ``MERGING``, fastboot refuses to erase
  ``userdata`` and ``metadata``.

``ab_get_merge_status()`` reads the status, ``ab_get_control()`` gives
read-only access to the metadata (for the fastboot ``slot-*`` variables and
for the bootmeth, which only stores new rollback indexes once the slot is
marked successful).

The same system space carries the MTE (``misc_memtag_message``) and kernel
command line (``misc_kcmdline_message``) requests of the OS; the Android
bootmeth applies them (``arm64.nomte``, ``kasan=``, ``binder.impl=rust``),
see ``boot/android_misc.c``.

References
----------

.. [1] https://source.android.com/devices/tech/ota/ab
.. [2] https://android.googlesource.com/platform/bootable/recovery/+/refs/heads/main/bootloader_message/include/bootloader_message/bootloader_message.h
.. [3] https://source.android.com/docs/core/ota/virtual_ab
