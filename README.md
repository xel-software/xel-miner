<h1><b>Miner		v0.9.4</b></h1>
<h1><b>ElasticPL 	v0.9.1</b></h1>

<i>This code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.</i>

This is a prototype of a miner for solving XEL work packages.  <b>The miner is not optimized in any way</b> as its purpose is to demonstrate all the functionality of ElasticPL and the workflow between the Miner & Elastic Node regardless of hardware or OS.

It is intended that other developers will improve upon the miner performance & functionality by creating versions for specific hardware / OS including all applicable optimizations.

<b>*** The GPU miner is highly experimental.  If you choose to use it, monitor your cards closely to ensure they don't overheat. ***</b>
________________________________________________________________________________________________

The miner build has been tested using GCC on Ubuntu 16.04 as well as MinGW32 (using GCC) on Windows 7/10.

Below are the steps I used to get the miner running on an Ubuntu server & Raspberry Pi.
<ul>
<li>sudo apt-get update</li>
<li>sudo apt-get install cmake libcurl4-openssl-dev libudev-dev screen libtool pkg-config libjansson-dev libssl-dev</li>
<li>git clone https://github.com/sprocket-fpga/xel_miner.git</li>
<li>cd xel_miner</li>
<li>cd build</li>
<li>cmake .. (note, if you are using OpenCL, then use "cmake .. -USE_OPENCL"</li>
<li>make install</li>
</ul>

<b>*** Don't forget to use "make install" and not just "make" ***</b>

To run the Miner using CPU:

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D

To run the Miner using GPU:

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D --opencl

Use "sudo ./xel_miner -h" to see a full list of options.

________________________________________________________________________________________________


<b>Credits</b>
<ul>
<li>The core of the miner is based on cpuminer</li>
<li>The ElasticPL / Work Package logic is based on the tireless efforts of Evil-Knievel</li>
<li>The Elastic project can be found here: https://github.com/OrdinaryDude</li>
</ul>