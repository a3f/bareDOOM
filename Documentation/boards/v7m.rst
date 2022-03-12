ARMv7-M MCU (Cortex-M) Support
==============================

barebox supports being built for some platforms also supported by nommu Linux.

Memory Use
----------

eXecute-In-Place (XIP) is employed in prebootloader. compressed barebox proper
is extracted from flash to the end of SDRAM.

As storage is usually quite limited, executing from SDRAM is favored over
XIP from flash, which would preclude compression.

Building barebox
----------------

There is a sole defconfig::

  make ARCH=arm CROSS_COMPILE=arm-none-eabi- mrproper
  make ARCH=arm CROSS_COMPILE=arm-none-eabi- v7m_defconfig
  make ARCH=arm CROSS_COMPILE=arm-none-eabi-

The resulting images will be placed under ``images/``::

  barebox-stm32f429-disco.img

ST-Link: Flashing barebox
-------------------------

Barebox can be bootstrapped and started via ST-Link as follows:

.. code-block:: sh

  Documentation/boards/stm32/flash.sh images/barebox-stm32f429-disco.img
