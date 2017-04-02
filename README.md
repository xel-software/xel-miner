<h1><b>DO NOT USE - Code is not usable until SN integration is complete</b></h1>


# xel_miner 0.9.1

<a href="https://scan.coverity.com/projects/ordinarydude-xel_miner">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/10948/badge.svg"/>
</a>  <img src="https://travis-ci.org/OrdinaryDude/xel_miner.svg?branch=master"/> [![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](https://github.com/OrdinaryDude/elastic-core/issues)

<i>This code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
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

To run the Miner using CPU

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D

To run the Miner using GPU (Note: Crypto & Big Integer ElasticPL jobs are not supported for GPU mining)

    sudo ./xel_miner -t <num_threads> -P <secret_phrase> -D --opencl

Use "sudo ./xel_miner -h" to see a full list of options.
________________________________________________________________________________________________

#Credits
<ul>
<li>The core of the miner is based on cpuminer</li>
<li>The ElasticPL / Work Package logic is based on the tireless efforts of Evil-Knievel</li>
<li>The Elastic project can be found here: https://github.com/OrdinaryDude</li>
</ul>