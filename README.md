<html>
<b>Enable your Pluto SDR to become a stand-alone OFDM transceiver with batman-adv mesh networking capabilities</b>
<BR>
<BR><B>Charon</B>  (Charon is named after one of the planet pluto's 5 known moons).
<BR>
<p>
The Charon project aims to enable those who own 2 or more Pluto SDR devices to
experiment with narrow-band OFDM channels and mesh networking. In the current
state, it supports OFDM with 64 sub-carriers, 16-QAM, and a data rate of
272-Kbps (after FEC coding rate) in an occupied bandwidth of 140KHz.  Due to
the use of the batman-adv protocol, there are no limitations on what
higher-layer traffic it supports. The mesh routing is accomplished at layer
2 (MAC addressing). Every wireless layer-2 transmission that is not broadcast
is acknowledged with user-configurable options for short and long transmissions
(<128 bytes is considered short).  Broadcast may be configured for 0 or more
re-transmissions (1 or more tx without ack). Host systems (computers attached
to the Pluto device) are not required to be batman-adv enabled to communicate
with other hosts or Pluto-devices on the network.  Up to 4 Pluto devices have
been tested with 3 host machines (one stand-alone pluto with no host). With
batman OGM broadcasts being broadcast on the 4 pluto devices, the TCP
throughput was good down to -100 dBm or less at around a peak of 117 Kbps (no
repeater hops).  A single repeater hop (if batman-adv decides to route that
way), reduces the TCP throughput to around 80 Kbps. More hops will further
reduce the TCP throughput and latency of other protocols. The Charon daemon may
be disabled from starting on power-up via a user-configurable u-boot
environment variable (via fw_setenv enable_charon) if normal Pluto SDR
functionality is desired (e.g. using GQRX, GNU-Radio, etc).
</p>
<BR>
<B>Plot of 3rd radio monitoring the on-air spectrum and constellation of 2 transceivers communicating via OFDM-64, QAM-16</B>
<img src="https://github.com/tvelliott/charon/blob/master/images/charon_receive_plot.gif">
<BR>
<B>Installation</B>
<BR>
<p>
The firmware for the Charon system may be installed by the normal means of
updating the Pluto SDR.  Find the pluto.frm binary image included in this
repository (build_pluto_image dir) and install just like any other pluto.frm
image.  
<BR>(e.g.  mount /dev/sdx /media; cp pluto.frm /media; eject /media )
</p>
<BR>
<B>Configuration</B>
<p>
The following DEFAULTS are used if not set manually with fw_setenv  (viewed
with fw_printenv).  A script is provided in the /root directory for setting
the defaults via fw_setenv.  See config.c for more insight on this.
<BR>
NOTE: the maxcpus environment variable must be set to enable the second cpu core.
</p>
<pre>
#define DEFAULT_ENABLE_CHARON 1   //set to zero if you don't want the charon daemon to start and use libiio resources
#define DEFAULT_FREQ_OFFSET_PPM 6.15 //frequency correction in parts-per-milllion  (e.g. @915 Mhz, 1ppm is 915 Hz).  Important to get the frequency of nodes very close for narrow OFDM.  Right now this has to be positive # due to limitations of fw_setenv
#define DEFAULT_OGM_INTERVAL 10000  //the default batman OGM routing messages are 1 second.  Here we back them off to 10 seconds due to limited bandwidth
#define DEFAULT_MAX_TCP_SEGS 2  //the tcp window size (size=1460 * max_tcp_segs) that we trick the host machines into seeing during SYN/SYN-ACK 
#define DEFAULT_MAX_SHORT_RETRANS 8  //frames < 128 bytes are retransmitted this many times until acked.  otherwise they are dropped
#define DEFAULT_MAX_LONG_RETRANS 1 //frames >= 128 bytes are retransmitted this many times until acked.  otherwise they are dropped 
#define DEFAULT_BCAST_RETRANS 1 //broadcast frames including batman-adv OGM messages are re-transmitted this many times  (1 retrans = 2 total transmissions)
#define DEFAULT_USB_BATMAN_IF 0  //for now just leave this at zero.  if you are interested in seeing some OGM messages, set to one. see config.c for more info
#define DEFAULT_ACK_DELAY_TIMEOUT 25000  //the time in micro-seconds to wait for acknowledge packet before re-transmitting
#define DEFAULT_SYMBOL_DELAY ((OFDM_M+CP_LEN+TAPER_LEN)*(DECIMATE_INTERPOLATE_FACTOR/4))  //the time in micro-seconds that the transmit will wait after packet reception before transmitting (in addition to wait for ack) 
#define DEFAULT_MAX_TCP_SHARE_BACKOFF (DEFAULT_SYMBOL_DELAY*12); //not used currently.  better option will be to monitor tcp sessions and dynamically adjust tcp window size to allow better TCP sharing of bandwidth
#define DEFAULT_TXRX_FREQ_HZ 915000000  //frequency for the current binary in Hz.  Currently this is limited to operating in the 902-928 Mhz and 2412-2462 Mhz bands.
#define DEFAULT_SAMPLE_FREQ_HZ 11200000  //sample rate is fixed fow now. Uses LTE20 MHz filter and decimates to rate of 1400000 Hz, further software decimated by 8x with floating point kaiser filter for occupied bandwidth of 140 KHz
#define DEFAULT_RF_BANDWIDTH_HZ 250000  //minimum rf bandwidth of the TIA filter on the Pluto  (if I understand correctly)
#define DEFAULT_TX_OUTPUT_POWER -10     //default output power is 100 micro-Watts.  Shouldn't cause too much harm to anything with this setting.  2nd harmonic @ 915 is nearly non-existent.  3rd show up, but very low
</pre>
<BR>
<p>
As mentioned already, the default u-boot variables for charon may be set with
the included script in root.  
<BR>
#root> sh set_charon_env.sh.  
<BR>
This will reset all defaults including the freq error in ppm.  Please delete
the line for freq_offset_ppm if you already set this based on measurements.
The frequency for narrow-band OFDM does need to be somewhat accurate.  You will
most likely need to measure the relative frequency error of your devices to get
this setting right.  If you don't have a spectrum analyzer, then you can try
adjusting the freq_offset_ppm by +/- 0.5 until it starts working... or better
yet use one of your plutos as a spectrum analyzer to measure the relative
frequency error of your other devices. The ppm frequency offset,  ip address,
and maxcpus are really all you need to set manuallly to get a network up and
running. The pluto ip address is used to generate a unique MAC address for the 
wireless interface, so no need to manually set that either. The maxcpus requirement
allows charon to run on a separate cpu core from the rest of the system. This
keeps samples from getting dropped.
<BR>
The config script in /root/ contains the following
<BR>
<pre>
fw_setenv attr_name compatible
fw_setenv attr_val ad9364
fw_setenv maxcpus
fw_setenv enable_charon 1
fw_setenv ref_correction_ppm 5.0
fw_setenv bat_ogm_interval 10000
fw_setenv max_long_retrans 1
fw_setenv max_short_retrans 8
fw_setenv bcast_retrans 1
fw_setenv max_tcp_segs 2
fw_setenv usb_batman_if 0
fw_setenv ack_delay_timeout 30000
fw_setenv symbol_delay_timeout 144
fw_setenv max_tcp_share_backoff 1728
fw_setenv freq_rxtx_hz 915000000 
fw_setenv sample_freq_hz 11200000
fw_setenv rf_bandwidth 250000 
fw_setenv tx_output_power_minus_dbm 10
</pre>
<BR>
</p>
<BR>
<B>Usage</B>
<p>
After installing Charon on all nodes and setting the individual IP adresses for
the device and host, you may want to see some output about what is going on.
To enable viewing of packet reception, transmission, signal level, quality,
etc, login to the device  
<BR>username: root
<BR>password: analog 
<BR>and restart the charon daemon:   <B>/etc/init.d/S100-start_charon restart  </B>
<BR>After that, you should start to see batman-adv OGM broadcasts being transmitted 
and received:

<pre>
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -77.4 dBm, rx EVM -22.7 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -82.6 dBm, rx EVM -21.7 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -88.4 dBm, rx EVM -21.9 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -78.9 dBm, rx EVM -20.9 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
RF_IN->TAPDEV_OUT_, len=54, rssi: -76.2 dBm, rx EVM -23.7 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -82.1 dBm, rx EVM -21.1 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
RF_IN->TAPDEV_OUT_, len=54, rssi: -76.5 dBm, rx EVM -27.9 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
RF_IN->TAPDEV_OUT_, len=54, rssi: -89.3 dBm, rx EVM -25.0 dB, in-gain: 65 dB, d-rate 272 Kbps, #sub-carriers=64, mod: qam16
TAPDEV_IN -> RF_OUT , len=54
</pre>
<BR>
Now try pinging one of the remotes.  It may take a few seconds (or more) before
ARP and OGM tables are able to resolve. Once packets starting getting
through, this delay is not incurred until a long period inactivity has passed.
(table entries expire). With the default Charon settings,  batman-adv OGM packets are
broadcast every 10 seconds to keep bandwidth usage to minimum. The
packet error rate remains very low down to less than -100 dBm. 
<BR>
The Pluto devices have been configured to run an iperf3 server.  You can
measure up/down TCP bandwidth with the following (assuming you have installed
iperf3 on your end-device host or you are running iperf3 from a prompt on a pluto device).
<BR>
<BR>iperf3 -c remote_node address  (client)
<BR>iperf3 -R -c remote_node address  (reverse-directionclient)
<BR>
<BR>example output:
<pre>
>iperf3 -c 192.168.2.2
  Connecting to host 192.168.2.2, port 5201
  [  4] local 192.168.2.10 port 35510 connected to 192.168.2.2 port 5201
  [ ID] Interval           Transfer     Bandwidth       Retr  Cwnd
  [  4]   0.00-1.00   sec  38.5 KBytes   315 Kbits/sec    1   14.3 KBytes       
  [  4]   1.00-2.00   sec  14.3 KBytes   117 Kbits/sec    0   14.3 KBytes       
  [  4]   2.00-3.00   sec  12.8 KBytes   105 Kbits/sec    1   9.98 KBytes       
  [  4]   3.00-4.00   sec  11.4 KBytes  93.5 Kbits/sec    2   2.85 KBytes       
  [  4]   4.00-5.00   sec  12.8 KBytes   105 Kbits/sec    0   4.28 KBytes       
  [  4]   5.00-6.00   sec  11.4 KBytes  93.5 Kbits/sec    1   2.85 KBytes       
  [  4]   6.00-7.00   sec  14.3 KBytes   117 Kbits/sec    0   4.28 KBytes       
  [  4]   7.00-8.00   sec  12.8 KBytes   105 Kbits/sec    0   4.28 KBytes       
  [  4]   8.00-9.00   sec  14.3 KBytes   117 Kbits/sec    0   4.28 KBytes       
  [  4]   9.00-10.00  sec  11.4 KBytes  93.5 Kbits/sec    1   4.28 KBytes       
  - - - - - - - - - - - - - - - - - - - - - - - - -
  [ ID] Interval           Transfer     Bandwidth       Retr
  [  4]   0.00-10.00  sec   154 KBytes   126 Kbits/sec    6             sender
  [  4]   0.00-10.00  sec   125 KBytes   103 Kbits/sec                  receiver
</pre>

</p>
<BR>
<B>Credits</B>
<p>
<BR>
GNU and Linux
<BR>
<BR>
Analog Devices:  Creating hardware and open-sourcing everything.  
<BR>
<BR>
Xilinx:  Open-sourcing much of their software and creating free-to-use tools. 
<BR>
<BR>
Joseph Gaeddert, The liquid-dsp project. http://liquidsdr.org/blog  If you have not seen this project
before and you have an interest in DSP, then you need to check it out.  The tutorials and code are
very good.  The entire project is a work of art.  Charon completely depends on this library.
<BR>
<BR>
batman-adv protocol
<BR>
https://www.kernel.org/doc/html/v4.15/networking/batman-adv.html
<BR>
https://www.open-mesh.org/projects/open-mesh/wiki
<BR>
<BR>
libfftw3  http://www.fftw.org/
<BR>
<BR>
libtuntap - forked and cross-compiled with .a library output ready to be linked against charon
<BR>
<BR>
libfec - default charon config does not use, but liquid-dsp and charon link against libfec. You can experiment with these convolutional FECs by changing ofdm_conf.h 
<BR>
</p>
<BR>
<B>Future Developement</B>
<BR>
<BR>
Add option for AES encryption.  In addition to wireless security, this will provide a means of partitioning receiver domains.
<BR>
<BR>
Dynamic scaling of TCP Window size in tcp_subs.c  This will require keeping track of TCP session passing
through the network.  This should enable better sharing of the limited bandwidth between multiple TCP
sessions.
<BR>
<BR>
Need to figure out how to optimize for higher data-rates.  
Currently QAM-64 works stand-alone, but due to optimization issues (samples getting dropped),
does not result in higher data throughput than QAM-16.  When doing loopback testing on a PC,
up to QAM-256 with much higher-order FEC was tested.
<BR>
<BR>
Use the CFO estimate (nco_crcf_get_frequency) of the OFDM preamble to tune the XO correction. (as per suggestion from tfcollins).
<BR>
<BR>
<B>For fun....</B>
<BR>
<BR>
<B>Plot of a 64-symbol constellation I decided to call Pi modulation  (utilizes arbitrary mapping available in liquid-dsp)</B>
<BR>
<img src="https://github.com/tvelliott/charon/blob/master/images/pi_modulation_small.gif">
<BR>
<BR>
<p>With a low distortion signal and enough SNR, it performs the same as QAM-64 for data rate.</p>
<BR>
<BR>
<B>Notes On Troubleshooting</B>
<BR>
<BR>
Make sure the frequencies of all nodes are the same.  Because the transmit and
receive are derived from the same reference, you can just monitor the transmit
output of each device and adjust for that observed error.  Note that the
setting for ppm set via "fw_setenv ref_correction_ppm 5.0" must be positive due
to an issue with setting negative values with fw_setenv.  You could just leave
the ppm correction set to zero and just adjust the tx/rx frequency for each
node "fw_setenv freq_rxtx_hz 915000000".
<BR>
<BR>
The AGC code took a while to get working well.  The AGC is adjusted with
a slow and fast timer to try and reduce EVM to around -20 dB or better.  It
starts off with the fast-attack and switches to manual.  This allows distant
and near nodes to communicate without issues.  In the current state,  the units
can be distant or right on top of each other, but if you do have issues try
separating them by at least a few feet to see if that helps.
<BR>
Regarding AGC:  After speaking with someone from Analog Devices, it sounds
like a better approach for the AGC would be to use a custom AGC setup to 
handle the specific waveforms being utilized.  The Pluto AGC is quite complex
(in a good way).  Once this gets sorted out, that should free up some more
cpu cycles for getting other things done.  (not done yet)
<BR>
<BR>
Make sure the "fw_setenv maxcpus" environment variable is set.  The charon
executable needs to run on a separate cpu core.
<BR>
<BR>
The ip-address of the wireless interface is configured by charon.  While charon
is running, you will see several network interfaces:   ofdm0: bat0: mesh-bridge: usb0: 
On a default pluto system, the ip-address is assigned to the usb0: interface.  Charon
sets up a bridge interface that the usb0 interface becomes part of.  Do not set the
ip address of the usb0: device, just the bridge.  Again, this is configured by charon
on startup, but just pointing out that the mesh-bridge: interface gets the ip assigned
while running the mesh network.  The mesh-bridge will still configure the host system
over the usb interface (as configured in config.txt) like the default pluto firmware.
<BR>
<BR>
<B>Setting up a build environment</B>
<BR>
<BR>
Go to analog devices plutosdr-fw github page and follow the instructions for building  https://github.com/analogdevicesinc/plutosdr-fw
<BR>
<BR>
You may want to make a backup of your fresh/default plutosdr-fw build at this point.
<BR>
<BR>
<pre>
cd plutosdr-fw
export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
export PATH=$PATH:/opt/Xilinx/SDK/2016.2/gnu/arm/lin/bin
export VIVADO_SETTINGS=/opt/Xilinx/Vivado/2016.4/settings64.sh

git clone https://github.com/tvelliott/charon.git
cp -fr charon/changes_to_plutosdr_fw_configs_rel_to_v28/* .

cd buildroot
make batctl
make bridge-utils
rm -fr output/build/fftw-3.3.7/
make fftw
make iperf3
make iproute2
make liquid-dsp
make tunctl
rm -fr output/build/util-linux-2.31.1/
make util-linux
cd ..

cd charon
mkdir third_party
cd third_party
git clone https://github.com/tvelliott/libtuntap.git
git clone https://github.com/tvelliott/libfec.git
cd ..
make clean
make
cp third_party/libfec/*.so ../buildroot/output/target/usr/lib
cp charon ../buildroot/output/target/usr/bin
cd ..
make
</pre>
<BR>
<BR>
You may need to edit the charon Makefile to point to the correct location/version for the Xilinx SDK.
At this point your pluto.frm (with charon) should be in the plutosdr-fw/build directory should be ready 
to install.
<BR>
<BR>
Once you get the charon executable to compile, then I have been just copying the charon executable 
to ../buildroot/output/target/usr/bin and re-building the plutosdr-fw image. (as shown in previous steps)
<BR>
Note that there are some other files that are also copied to the buildroot/output from the 
"cp -fr charon/changes_to_plutosdr_fw_configs_rel_to_v28 ." step.  These probably get wiped out
if you do a clean. In that case, you may need to re-copy them before building the pluto firmware image.
<BR>
<BR>
At this point, you can login to the device and run the script found in /root to set the default env configuration  (maxcpus is required)
<BR>
<BR>
Sorry, the build procedure is kind of a hack I know, but hopefully that should get you up and experimenting with it. 
<BR>
<BR>
If you make some changes and get a reliable increase in throughput or other enhancements, please feel free to send a pull request.

</html>
