# Phos

Phos is a simplified scheduler for embedded devices based on the ARM
Cortex-M0.  The design and the inter-process communication mechanism
are modelled on the internal structure and Andy Tanenbaum's Minix
implementation of Unix.  Phos can be built on a Linux system using the
standard version of GCC for Cortex-M0 devices, arm-none-eabi-gcc.

Initial support is for the Nordic nrF51822 processor on the BBC
micro:bit. This repository supports a collaborative development
effort, where we aim to port Phos to one or more other boards
(beginning with the Freescale/NXP FRDM-KL25Z),
and to add driver processes for other peripherals (beginning with I2C).

Project notes are on the wiki at
https://spivey.oriel.ox.ac.uk/corner/The_Phos_project.

