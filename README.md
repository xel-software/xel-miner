<h1><b>Only Use For Standalone ElasticPL Testing!</b></h1>

# xel_miner 	0.9.2
# ElasticPL 	0.9.1

<i>This code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.</i>

This is a prototype of a miner for solving XEL work packages.  <b>The miner is not optimized</b> in any way as its purpose is to demonstrate all the functionality of ElasticPL and the workflow between the Miner & Elastic Node.

It is intended that other developers will improve on the miner performance & functionality by creating versions for specific hardware / OS including all applicable optimizations.

<b>*** The GPU miner is highly experimental.  If you choose to use it, monitor your cards closely to ensure they don't overheat. ***</b>

The miner build has been tested using GCC in Linux as well as MinGW32 (using GCC) on Windows.

Below are the steps I used to get the miner running on my Raspberry Pi.
<ul>
<li>sudo apt-get update</li>
<li>sudo apt-get install cmake libcurl4-openssl-dev libudev-dev screen libtool pkg-config libjansson-dev libssl-dev</li>
<li>git clone https://github.com/sprocket-fpga/xel_miner.git</li>
<li>cd xel_miner</li>
<li>cd build</li>
<li>cmake ..</li>
<li>make install</li>
</ul>

<b>*** Don't forget to use "make install" and not just "make" ***</b>

To run the Miner using CPU:

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D

To run the Miner using GPU:

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D --opencl

Use "sudo ./xel_miner -h" to see a full list of options.

________________________________________________________________________________________________

<b>Donations are welcome to support my work on this project:</b>

	XEL: XEL-AZJK-TAVH-3KN3-9EZSH
________________________________________________________________________________________________


#Credits
<ul>
<li>The core of the miner is based on cpuminer</li>
<li>The ElasticPL / Work Package logic is based on the tireless efforts of Evil-Knievel</li>
<li>The Elastic project can be found here: https://github.com/OrdinaryDude</li>
</ul>