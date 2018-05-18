<html>
<b>Enable your Pluto SDR to become a stand-alone OFDM transceiver with batman-adv mesh networking capabilities</b>
<BR>
<BR><B>Charon</B>  (Charon is named after one of the planet pluto's 5 known moons).
<BR>
<p>
The Charon project aims to enable those who own 2 or more Pluto SDR devices to
experiment with narrow-band OFDM channels and mesh networking. In the current
state, it supports OFDM with 64 sub-carriers, 16-QAM, and a data rate of
272-Kbps (after FEC coding rate) in an occupied bandwidth of 140KHz.  I believe
this is the first open-source example of such a system working on real
hardware. Due to the use of the batman-adv protocol, there are no limitations
on what higher-layer traffic it supports. The mesh routing is accomplished at
layer 2 (MAC addressing). Every wireless layer-2 transmission that is not
broadcast is acknowledged with user-configurable options for short and long
transmissions (<128 bytes is considered short).  Broadcast may be configured
for 0 or more re-transmissions (1 or more tx without ack). Host systems
(computers attached to the Pluto device) are not required to be batman-adv
enabled to communicate with other hosts or Pluto-devices on the network.  Up to
4 Pluto devices have been tested with 3 host machines (one stand-alone pluto
with no host). Even with batman OGM broadcasts being broadcast on 4 devices,
the TCP throughput is good down to -100 dBm or less at around a peak of 117
Kbps (no repeater hops).  A single repeater (if batman-adv decides to route
that way), reduces the TCP throughput to around 80 Kbps. More hops will
further reduce the TCP throughput and latency of other protocols. The Charon
daemon may be disabled via a user-configurable u-boot environment variable (via
fw_setenv) if normal Pluto SDR functionality is desired (e.g. using GQRX,
GNU-Radio, etc).
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
#define DEFAULT_SAMPLE_FREQ_HZ 1400000  //sample rate given to pluto with default of the LTE 1.4 MHz filter / decimation of 4x.  Charon decimates/interpolates this by another factor of 8x
#define DEFAULT_RF_BANDWIDTH_HZ 250000  //minimum rf bandwidth of the TIA filter on the Pluto  (if I understand correctly)
#define DEFAULT_TX_OUTPUT_POWER -10     //default output power is 100 micro-Watts.  Shouldn't cause too much harm to anything with this setting.  2nd harmonic @ 915 is nearly non-existent.  3rd show up, but very low
</pre>
<BR>
<p>
As mentioned already, the default u-boot variables for charon may be set with
the included script in root.  #root>sh set_charon_env.sh.  NOTE: This will
reset all defaults including the freq error in ppm.  Please delete the line for
freq_offset_ppm if you already set this based on measurements.  The frequency
for narrow-band OFDM does need to be somewhat accurate.  You will most likely
need to measure the frequency error of your device to get this setting right.
(probably within +/- 1 Khz, preferably better)  If you don't have a spectrum
analyzer, then you can try adjusting the freq_offset_ppm by +/- 0.5 until it
starts working.  The ppm frequecy offset,ipaddr (set via config.txt in
Analog Devices documentation), and maxcpus are really all you need to set
manuallly to get a network up and running.  The ipaddr is used to generate
a unique MAC address for the wireless interface, so no need to manually set
that either.  The maxcpus requirement allows charon to run on a separate cpu core 
from the rest of the system.  This keeps samples from getting dropped.
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
fw_setenv sample_freq_hz 1400000 
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
To enable viewing of packet recepton transmission, signal level, quality,
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
Now try pinging one of the remotes.  It may take a few seconds (or more) before ARP request/reply happens in both directions. With
the default Charon settings,  batman-adv OGM packets are only broadcast every 10 seconds to keep bandwidth usage to minimum. The 
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
Still need to fork the Analog Devices pluto firmware repo and add all the configuration changes / scripts related to Charon
<BR>
<BR>
Need to figure out how to optimize for higher data-rates.  
Currently QAM-64 works stand-alone, but due to optimization issues (samples getting dropped),
does not result in higher data throughput than QAM-16.  When doing loopback testing on a PC,
up to QAM-256 with much higher-order FEC was tested.
<BR>
<BR>
<B>For fun....</B>
<BR>
<B>Plot of a 64-symbol constellation I decided to name PI modulation  (utilizes arbitrary mapping available in liquid-dsp) 
<img src="https://github.com/tvelliott/charon/blob/master/images/pi_modulation_small.gif">
<BR>
<BR>
With a low distortion signal and enough SNR, it performs the same as QAM-64 for data rate.

</html>
