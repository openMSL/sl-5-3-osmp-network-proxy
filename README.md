# OSMP Network Proxy

[![Credibility Assessment](../../actions/workflows/credibility_assessment.yml/badge.svg)](https://github.com/openMSL/sl-1-0-sensor-model-repository-template/actions/workflows/credibility_assessment.yml)

This Network Proxy FMU can receive SensorView and SensorData via TCP/IP using ZeroMQ.
The received data is directly given to the FMU output.
The proxy can also send SensorView oder SensorData received as FMU input via TCP/IP to a given IP address and port.
<br><br>

< Eye-catcher Image >
<!--img src="doc/img/model_video.gif" width="800" /-->

## Parameterization

The following FMI parameters can be set:

| Type    | Parameter  | Description                                |
|---------|------------|--------------------------------------------|
| Boolean | `sender`   | Set if Proxy shall send data via TCP/IP    |
| Boolean | `receiver` | Set if Proxy shall receive data via TCP/IP |
| String  | `ip`       | IP address of TCP connection               |
| String  | `port`     | Port of TCP connection                     |

## Interface

The Proxy can receive SensorView or SensorData as input.

## Build Instructions

### Build Model in Ubuntu 18.04 / 20.04

1. Clone this repository **with submodules**:

    ```bash
    git clone https://github.com/openMSL/your-model.git --recurse-submodules
    ```

2. Install dependencies:

   ```bash
   sudo apt install libzmq3-dev
   ```

3. Build the model by executing in the extracted project root directory:

    ```bash
    mkdir cmake-build
    cd cmake-build
    # If FMU_INSTALL_DIR is not set, CMAKE_BINARY_DIR is used
    cmake -DCMAKE_BUILD_TYPE=Release -DFMU_INSTALL_DIR:PATH=/tmp ..
    make
    ```

4. Take FMU from `FMU_INSTALL_DIR`
