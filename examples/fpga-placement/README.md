<!--
SPDX-FileCopyrightText: 2026 Donovan Worrell and Just a Geek LLC
SPDX-License-Identifier: Apache-2.0
Catalyst-File-ID: urn:uuid:b959c5cd-fde6-4ef1-a130-92b455d37bc7
-->

# FPGA Placement Reference Adapter

`fpga_placement.c` searches x/y position, pipeline depth, and memory bank over
an explicit 8×8 integer fixture. Out-of-envelope placement, pipeline, and bank
values are hard invalid. Timing above the modeled target and LUT use above the
modeled budget are visible soft penalties already included in `fitness.total`.
The consumer stop callback requests successful termination after generation
three, proving application-controlled bounded stopping and observer order.

This fixture is not a hardware tool. It invokes no vendor placer, router,
timing engine, simulator, bitstream generator, or device. It makes no physical
timing, resource, safety, or deployability claim beyond its stated integer
model.
