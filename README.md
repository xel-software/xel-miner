----
# Welcome to XEL!

XEL is a decentralized supercomputer based on cryptography and blockchain technology.

----
## disclaimer

XEL CORE / XELINE IS OPEN-SOURCE SOFTWARE RUNNING ON THE MAIN-NET BUT IS STILL CONSIDERED "BETA" AND MAY CONTAIN BUGS, SOME OF WHICH MAY HAVE SERIOUS CONSEQUENCES. WE THEREFORE DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK. USE THE SOFTWARE AND THE INFORMATION PRESENTED HERE AT OUR OWN RISK.

*This code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.*

This is a prototype of a miner for solving XEL work packages.  **The miner is not optimized in any way** as its purpose is to demonstrate all the functionality of ElasticPL and the workflow between the Miner & Elastic Node regardless of hardware or OS.

It is intended that other developers will improve upon the miner performance & functionality by creating versions for specific hardware / OS including all applicable optimizations.

**The GPU miner is highly experimental.  If you choose to use it, monitor your cards closely to ensure they don't overheat.**

----
## Run XEL Miner from sources

Miner		v0.9.6
ElasticPL 	v0.9.1

**The miner build has been tested using GCC on Ubuntu 16.04 as well as MinGW32 (using GCC) on Windows 7/10.**

```
apt-get update
apt-get install -y cmake libcurl4-openssl-dev libudev-dev screen libtool pkg-config libjansson-dev libssl-dev
git clone --depth 1 https://github.com/xel-software/xel-miner
cd xel-miner
```

if you **don't** want to use OpenCL :
```
cmake .
make install
```

if you want to use OpenCL :
```
cmake .  -USE_OPENCL
make install
```


### To run the Miner using CPU:

`./xel_miner -t <num_threads> -P <secret_phrase> -D`

### To run the Miner using GPU:

`./xel_miner -t <num_threads> -P <secret_phrase> -D --opencl`

use `./xel_miner -h` to see a full list of options.


----
## Run XEL Miner from docker installer

check the dedicated git project : https://github.com/xel-software/xel-installer-docker


----
## Improve it

  - we love **pull requests**
  - we love issues (resolved ones actually ;-) )
  - in any case, make sure you leave **your ideas**
  - assist others on the issue tracker
  - **review** existing code and pull requests

----
## Troubleshooting

  - UI Errors or Stacktraces?
    - report on github

----
## Further Reading

  - on discord : https://discord.gg/5YhuSzd


----
## Credits
  - The core of the miner is based on cpuminer
  - The ElasticPL / Work Package logic is based on the tireless efforts of Evil-Knievel
  - The Elastic project can be found here: https://github.com/xel-software
