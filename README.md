# 100 GbE UDP

This branch implements the Network layer of [VNx](https://github.com/Xilinx/xup_vitis_network_example), for the full stack (TCP) refer to the [master](https://github.com/hpcn-uam/100G-fpga-network-stack-core) branch

## Prerequisites

*Make sure that Vitis HLS is in the PATH and you are running a supported version*

- Vitis HLS 2021.1 or newer

If you are not sure what version is running execute the following command:

```
vitis_hls -version
```

## Getting Started

The makefile process is automated to target Virtex Ultrascale+ and Virtex Ultrascale+ HBM devices.

### Virtex Ultrascale+

Execute `make` in the root folder, a new folder named `synthesis_results_noHBM` will be created. After the folder creation the project will launch the Synthesis of the different IP-Cores. Be patient this may take around 10 minutes.

### Virtex Ultrascale+ HBM

Execute `make hbm` in the root folder, a new folder named `synthesis_results_HBM` will be created. After the folder creation the project will launch the Synthesis of the different IP-Cores. Be patient this may take around 10 minutes.

## Explore the IP-Cores

You can check the synthetized project under the folder `synthesis_results_noHBM` or `synthesis_results_HBM`. For instance, if you want to open the UDP IP

```
vivado_hls -p synthesis_results_HMB/UDP_prj/
```


## Citation
If you use this stack or the checksum computation in your project please cite one of the following papers and/or link to the github project:

```
@inproceedings{sutter2018fpga,
    title={{FPGA-based TCP/IP Checksum Offloading Engine for 100 Gbps Networks}},
    author={Sutter, Gustavo and Ruiz, Mario and L{\'o}pez-Buedo, Sergio and Alonso, Gustavo},
    booktitle={2018 International Conference on ReConFigurable Computing and FPGAs (ReConFig)},
    year={2018},
    organization={IEEE},
    doi={10.1109/RECONFIG.2018.8641729},
    ISSN={2640-0472},
    month={Dec},
}
@INPROCEEDINGS{ruiz2019tcp, 
    title={{Limago: an FPGA-based Open-source 100~GbE TCP/IP Stack}}, 
    author={Ruiz, Mario and Sidler, David and Sutter, Gustavo and Alonso, Gustavo and L{\'o}pez-Buedo, Sergio},
    booktitle={{2019 29th International Conference on Field Programmable Logic and Applications (FPL)}}, 
    year={2019},
    month={Sep},
    pages={286-292}, 
    organization={IEEE},
    doi={10.1109/FPL.2019.00053},
    ISSN={1946-147X}, 
}
```

## License

This project is a collaboration between the Systems Group of ETH Zürich, Switzerland and HPCN Group of UAM, Spain. Furthermore, the starting point of this implementation is the [Scalable 10Gbps TCP/IP Stack Architecture for Reconfigurable Hardware](https://ieeexplore.ieee.org/abstract/document/7160037) by Sidler, D et al. The original implementation can be found in their [github](https://github.com/fpgasystems/fpga-network-stack).

For this project we keep the BSD 3-Clause License

```
BSD 3-Clause License

Copyright (c) 2019, 
HPCN Group, UAM Spain (hpcn-uam.es)
Systems Group, ETH Zurich (systems.ethz.ch)
All rights reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```