
PTP daemon implementations
==========================

.. contents::

ptpv2d-bartky-googlecode
------------------------
:homepage: http://code.google.com/p/ptpv2d/
:scm: http://ptpv2d.googlecode.com/svn/trunk
:licence: GPL
:dates: 2010-03-16 to 2011-07-10

Includes AS for freescale mpc831x

    This software implements IEEE 1588 and 802.1AS Precision Timing
    Protocol (PTP) and is a derivative work of the sourceforge.net
    ptpd daemon software.  This software is implemented to
    support IEEE 1588 PTP version 1 and version 2 and also
    IEEE 802.1AS.

    For ptpv2d, the source base used to create the this software
    was the sourceforge.net ptpd version v1rc1.

ptpd-sourceforge
----------------
:homepage: http://ptpd.sourceforge.net/
:scm: https://ptpd2.svn.sourceforge.net/svnroot/ptpd/trunk
:licence: 2 Clause BSD Open Source License
:dates: 2007 to 2011-02-22

..
        The PTP daemon (PTPd) implements the Precision Time protocol (PTP)
         as defined by the relevant IEEE 1588 standard. 
         PTP Version 1 implements IEEE-1588-2002, and PTP Version 2 implements IEEE-1588-2008. 
         PTP was developed to provide very precise time coordination of LAN connected computers.

        PTPd is a complete implementation of the IEEE 1588 specification 
        for a standard (non-boundary) clock. PTPd has been tested with 
        and is known to work properly with other IEEE 1588 implementations. 
        The source code for PTPd is freely available under a BSD-style license. 
        Thanks to contributions from users, PTPd is becoming an increasingly 
        portable, interoperable, and stable IEEE 1588 implementation.

        PTPd can run on most 32-bit or 64-bit, little- or big-endian processors. 
        It does not require an FPU, so it is great for embedded processors. 
        PTPd currently runs on Linux, uClinux, FreeBSD, and NetBSD. 
        It should also be easy to port to other platforms.

        PTPd is free. Everyone is welcome to use and contribute to PTPd. 

ptpd2-sourceforge
-----------------
:homepage: http://sourceforge.net/projects/ptpd2/
:scm: https://ptpd2.svn.sourceforge.net/svnroot/ptpd2
:licence: BSD-like
:dates: 2011-02-20 to 2011-03-25

..

    2009-2010 George V. Neville-Neil, Steven Kreuzer, Gael Mace, Alexandre Van Kempen
    2005-2008 Kendall Correll, Aidan Williams

    RELEASE_NOTES for PTPd V2 ()

    v2rc1, January 2010:

    This is the first public source code release of PTPd implementing PTP v2 as
    defined by 'IEEE Std 1588-2008'.
    The source has been derivated from ptpd v1.0.0, compiled with gcc and tested
    on x86 platforms connected on an Ethernet infrastructure using a Hirschman 1588v2
    compliant Ethernet switch configured in Transparent Clock mode.
    (http://www.hirschmann.com/en/).

    This implementation supports both E2E and P2P mode with an ordinary clock on
    master/slave end-points.
    It support the 802.1 AS profile (http://www.ieee802.org/1/pages/802.1as.html)

    Please, have a look on the short comparison document 'IEEE1588v1_vs_IEEE1588v2.pdf'
    in the 'Doc' subdirectory, for a overview of the main differences between the two
    versions of the IEEE1588 standard and the impact on the implementation.


ptpd2-ti-patched
----------------
:homepage: http://processors.wiki.ti.com/index.php/TI81XX_PSP_Ethernet_Switch_User_Guide#IEEE_1588.2F802.1AS_PTP_Support
:scm: http://processors.wiki.ti.com/index.php/File:Ptpd.tar.gz
:licence: BSD-like
:dates: 2011-10-19

Derived from ptpd2-sourceforge + bartky 802.1AS stuff

Adds ethernet mode

    Modified the PTPd2 source from source forge to support 802.1AS protocol
    by the reference from the source from code.google.com as it supports
    802.1AS but doesn't support POSIX hardware clock.

linuxptp
--------
:homepage: http://linuxptp.sourceforge.net/
:scm: git://git.code.sf.net/p/linuxptp/code
:licence:  GPLv2+
:dates: 2011-11-02 to 2012-03-21

..

  This software is an implementation of the Precision Time Protocol
  (PTP) according to IEEE standard 1588 for Linux. The dual design
  goals are to provide a robust implementation of the standard and to
  use the most relevant and modern Application Programming Interfaces
  (API) offered by the Linux kernel. Supporting legacy APIs and other
  platforms is not a goal.

Needs kernel PTP API first defined in kernel v3.0

ptp-ohwr
--------
:homepage:  http://www.ohwr.org/projects/pptp
:scm: git://ohwr.org/white-rabbit/pptp.git
:licence: LGPLv2.1 or later (diagnostics GPLv2)
:dates: 2011-12-05 to 2012-01-26

..

    Alessandro Rubini for CERN, 2011 -- GNU LGPL v2.1 or later
    Aurelio Colosimo for CERN, 2011 -- GNU LGPL v2.1 or later

    Many source files (mostly those regarding the algorithm itself) are based on the
    PTPd project, v.2.1.0 (see http://ptpd.sourceforge.net/)

    This project is concerned with writing a portable PTP daemon, to be
    used in White Rabbit and possibly other projects. The code base should
    be able to build a standard PTP and an extended one, running either
    under an operating system or as a freestanding application on bare metal.


xmos avb
--------
:homepage: http://www.xmos.com/applications/avb
:scm: git://github.com/xcore/sw_avb.git
:licence: BSD-like

..

    Implementation for XMOS parallel processor. Mix of C and XC language.

Xilinx AVB logicore
-------------------
:homepage: http://www.xilinx.com/products/intellectual-property/DO-DI-EAVB-EPT.htm
:scm: N/A
:licence: Proprietary

..

        The Ethernet Audio Video Endpoint LogiCORE provides a flexible solution
        to enhance standard Ethernet MAC functionality. The functionality provides
        prioritized channels through an existing MAC which are designed to provide
        a reliable, low latency, Quality of Service for streaming video and audio
        data. The LogiCORE is designed to emerging P802.1AS and P802.1 Qav
        standards for the Audio/Video Bridging (AVB) Task Group.
        It supports AVB Endpoint talker/listener functionality, seamless
        connection to the Xilinx Tri-Mode Ethernet MAC at speeds of 100 Mb/s and 1 G/s,
        and driver support for 802.1 AS implementation.

This or some variation is probably what is in the LabX Titanium 411 AVB switch.

