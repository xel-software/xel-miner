<h1><b>DO NOT USE!</b></h1>

Please use the fork create by EK at https://github.com/OrdinaryDude/elastic-miner


# xel_miner 	0.9.1
# xel_supernode	0.1

<i>This code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.</i>

This is a prototype of a miner for solving XEL work packages.  The miner is still in the early stages of development...it is simply a prototype that attempts demonstrate all the functionality of an XEL miner.

<b>*** The GPU miner is highly experimental.  If you choose to use it, monitor your cards closely to ensure they don't overheat. ***</b>

The miner build has been tested using GCC in Linux as well as MinGW32 (using GCC) on Windows.

Below are the steps I used to get the miner running on my Raspberry Pi.
<ul>
<li>sudo apt-get update</li>
<li>sudo apt-get install cmake libcurl4-openssl-dev libudev-dev screen libtool pkg-config libjansson-dev libgmp3-dev</li>
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

To run the SuperNode backend:

    sudo ./xel_miner -S -D

Use "sudo ./xel_miner -h" to see a full list of options.

________________________________________________________________________________________________

<b>Donations are welcome to support my work on this project:</b>

	BTC: 1N9dkpttDi7HncRd1q4t8MfFxCLvmLxAGU
	XEL: XEL-AZJK-TAVH-3KN3-9EZSH
________________________________________________________________________________________________


#Credits
<ul>
<li>The core of the miner is based on cpuminer</li>
<li>The ElasticPL / Work Package logic is based on the tireless efforts of Evil-Knievel</li>
<li>The Elastic project can be found here: https://github.com/OrdinaryDude</li>
</ul>