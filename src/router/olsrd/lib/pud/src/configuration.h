/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 *
 * (c) by the OLSR project
 *
 * See our Git repository to find out who worked on this file
 * and thus is a copyright holder on it.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#ifndef _PUD_CONFIGURATION_H_
#define _PUD_CONFIGURATION_H_

/* Plugin includes */
#include "gpsdclient.h"

/* OLSR includes */
#include "olsrd_plugin.h"
#include "olsr_types.h"

/* System includes */
#include <stdbool.h>
#include <stddef.h>
#include <OlsrdPudWireFormat/wireFormat.h>

/*
 * Global Parameters
 */

/** The default value of the nodeIdType plugin parameter */
#define PUD_NODE_ID_TYPE_DEFAULT				PUD_NODEIDTYPE_IPV4

NodeIdType getNodeIdTypeNumber(void);

/** The name of the nodeId plugin parameter */
#define PUD_NODE_ID_NAME						"nodeId"

unsigned char * getNodeId(size_t *length);
nodeIdBinaryType * getNodeIdBinary(void);
int setNodeId(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * Position Input File Parameters
 */

/** The name of the positionFile plugin parameter */
#define PUD_POSFILE_NAME						"positionFile"

char * getPositionFile(void);
int setPositionFile(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the positionFilePeriod plugin parameter */
#define PUD_POSFILEPERIOD_NAME    "positionFilePeriod"

/** The default value of the positionFilePeriod plugin parameter */
#define PUD_POSFILEPERIOD_DEFAULT ((unsigned long long)0)

/** The minimal value of the positionFilePeriod plugin parameter */
#define PUD_POSFILEPERIOD_MIN     ((unsigned long long)1000)

/** The maximal value of the positionFilePeriod plugin parameter */
#define PUD_POSFILEPERIOD_MAX     ((unsigned long long)320000000)

unsigned long long getPositionFilePeriod(void);
int setPositionFilePeriod(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * TX Parameters
 */

/** The name of the transmit non-OLSR interfaces plugin parameter */
#define PUD_TX_NON_OLSR_IF_NAME					"txNonOlsrIf"

bool isTxNonOlsrInterface(const char *ifName);
int addTxNonOlsrInterface(const char *value, void *data,
		set_plugin_parameter_addon addon);
unsigned int getTxNonOlsrInterfaceCount(void);
unsigned char * getTxNonOlsrInterfaceName(unsigned int index);

/** The name of the transmit multicast address plugin parameter */
#define PUD_TX_MC_ADDR_NAME						"txMcAddr"

/** The default value of the transmit multicast address plugin parameter fro IPv4*/
#define PUD_TX_MC_ADDR_4_DEFAULT				"224.0.0.224"

/** The default value of the transmit multicast address plugin parameter for IPv6 */
#define PUD_TX_MC_ADDR_6_DEFAULT				"FF02:0:0:0:0:0:0:1"

union olsr_sockaddr * getTxMcAddr(void);
int
setTxMcAddr(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the transmit multicast port plugin parameter */
#define PUD_TX_MC_PORT_NAME          			"txMcPort"

/** The default value of the transmit multicast port plugin parameter */
#define PUD_TX_MC_PORT_DEFAULT          		2240

unsigned short getTxMcPort(void);
int
setTxMcPort(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the transmit multicast time-to-live plugin parameter */
#define PUD_TX_TTL_NAME							"txTtl"

/** The default value of the transmit multicast time-to-live plugin parameter */
#define PUD_TX_TTL_DEFAULT						1

unsigned char getTxTtl(void);
int setTxTtl(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the transmit multicast NMEA message prefix plugin parameter */
#define PUD_TX_NMEAMESSAGEPREFIX_NAME			"txNmeaMessagePrefix"

/** The default value of the transmit multicast NMEA message prefix plugin parameter */
#define PUD_TX_NMEAMESSAGEPREFIX_DEFAULT		"NBSX"

unsigned char * getTxNmeaMessagePrefix(void);
int setTxNmeaMessagePrefix(const char *value, void *data,
		set_plugin_parameter_addon addon);

/*
 * Position Output File Parameters
 */

/** The name of the positionOutputFile plugin parameter */
#define PUD_POSOUTFILE_NAME            "positionOutputFile"

char * getPositionOutputFile(void);
int setPositionOutputFile(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * Uplink Parameters
 */

/** The name of the uplink address plugin parameter */
#define PUD_UPLINK_ADDR_NAME					"uplinkAddr"

/** The default value of the uplink address plugin parameter for IPv4*/
#define PUD_UPLINK_ADDR_4_DEFAULT				"0.0.0.0"

/** The default value of the uplink address plugin parameter for IPv6 */
#define PUD_UPLINK_ADDR_6_DEFAULT				"0:0:0:0:0:0:0:0"

bool isUplinkAddrSet(void);
union olsr_sockaddr * getUplinkAddr(void);
int
setUplinkAddr(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the uplink port plugin parameter */
#define PUD_UPLINK_PORT_NAME          			"uplinkPort"

/** The default value of the uplink port plugin parameter */
#define PUD_UPLINK_PORT_DEFAULT          		2241

unsigned short getUplinkPort(void);
int
setUplinkPort(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * Downlink Parameters
 */

/** The name of the downlink port plugin parameter */
#define PUD_DOWNLINK_PORT_NAME          		"downlinkPort"

/** The default value of the downlink port plugin parameter */
#define PUD_DOWNLINK_PORT_DEFAULT          		2242

unsigned short getDownlinkPort(void);
int
setDownlinkPort(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * OLSR Parameters
 */

/** The name of the OLSR multicast time-to-live plugin parameter */
#define PUD_OLSR_TTL_NAME						"olsrTtl"

/** The default value of the OLSR multicast time-to-live plugin parameter */
#define PUD_OLSR_TTL_DEFAULT					64

unsigned char getOlsrTtl(void);
int setOlsrTtl(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * Update Parameters
 */

/** The name of the stationary update interval plugin parameter */
#define PUD_UPDATE_INTERVAL_STATIONARY_NAME		"updateIntervalStationary"

/** The default value of the stationary update interval plugin parameter */
#define PUD_UPDATE_INTERVAL_STATIONARY_DEFAULT	60

unsigned long long getUpdateIntervalStationary(void);
int setUpdateIntervalStationary(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the moving update interval plugin parameter */
#define PUD_UPDATE_INTERVAL_MOVING_NAME			"updateIntervalMoving"

/** The default value of the moving update interval plugin parameter */
#define PUD_UPDATE_INTERVAL_MOVING_DEFAULT		5

unsigned long long getUpdateIntervalMoving(void);
int setUpdateIntervalMoving(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the uplink stationary update interval plugin parameter */
#define PUD_UPLINK_UPDATE_INTERVAL_STATIONARY_NAME	"uplinkUpdateIntervalStationary"

/** The default value of the uplink stationary update interval plugin parameter */
#define PUD_UPLINK_UPDATE_INTERVAL_STATIONARY_DEFAULT	180

unsigned long long getUplinkUpdateIntervalStationary(void);
int setUplinkUpdateIntervalStationary(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the uplink moving update interval plugin parameter */
#define PUD_UPLINK_UPDATE_INTERVAL_MOVING_NAME		"uplinkUpdateIntervalMoving"

/** The default value of the uplink moving update interval plugin parameter */
#define PUD_UPLINK_UPDATE_INTERVAL_MOVING_DEFAULT		15

unsigned long long getUplinkUpdateIntervalMoving(void);
int setUplinkUpdateIntervalMoving(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the gateway determination interval plugin parameter */
#define PUD_GATEWAY_DETERMINATION_INTERVAL_NAME			"gatewayDeterminationInterval"

/** The default value of the gateway determination interval plugin parameter */
#define PUD_GATEWAY_DETERMINATION_INTERVAL_DEFAULT		1

unsigned long long getGatewayDeterminationInterval(void);
int setGatewayDeterminationInterval(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the moving speed threshold plugin parameter */
#define PUD_MOVING_SPEED_THRESHOLD_NAME			"movingSpeedThreshold"

/** The default value of the moving speed threshold plugin parameter */
#define PUD_MOVING_SPEED_THRESHOLD_DEFAULT		9

unsigned long long getMovingSpeedThreshold(void);
int setMovingSpeedThreshold(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the moving distance threshold plugin parameter */
#define PUD_MOVING_DISTANCE_THRESHOLD_NAME		"movingDistanceThreshold"

/** The default value of the moving distance threshold plugin parameter */
#define PUD_MOVING_DISTANCE_THRESHOLD_DEFAULT	50

unsigned long long getMovingDistanceThreshold(void);
int setMovingDistanceThreshold(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the DOP multiplier plugin parameter */
#define PUD_DOP_MULTIPLIER_NAME		"dopMultiplier"

/** The default value of the DOP multiplier plugin parameter */
#define PUD_DOP_MULTIPLIER_DEFAULT	2.5

double getDopMultiplier(void);
int setDopMultiplier(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the default HDOP plugin parameter */
#define PUD_DEFAULT_HDOP_NAME		"defaultHdop"

/** The default value of the default HDOP plugin parameter */
#define PUD_DEFAULT_HDOP_DEFAULT	50

unsigned long long getDefaultHdop(void);
int setDefaultHdop(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the default VDOP plugin parameter */
#define PUD_DEFAULT_VDOP_NAME		"defaultVdop"

/** The default value of the default VDOP plugin parameter */
#define PUD_DEFAULT_VDOP_DEFAULT	50

unsigned long long getDefaultVdop(void);
int setDefaultVdop(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the average depth plugin parameter */
#define PUD_AVERAGE_DEPTH_NAME					"averageDepth"

/** The default value of the average depth plugin parameter */
#define PUD_AVERAGE_DEPTH_DEFAULT				5

unsigned long long getAverageDepth(void);
int setAverageDepth(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the hysteresis count to stationary plugin parameter */
#define PUD_HYSTERESIS_COUNT_2STAT_NAME			"hysteresisCountToStationary"

/** The default value of the hysteresis count to stationary plugin parameter */
#define PUD_HYSTERESIS_COUNT_2STAT_DEFAULT		17

unsigned long long getHysteresisCountToStationary(void);
int setHysteresisCountToStationary(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the hysteresis count to moving plugin parameter */
#define PUD_HYSTERESIS_COUNT_2MOV_NAME			"hysteresisCountToMoving"

/** The default value of the hysteresis count to moving plugin parameter */
#define PUD_HYSTERESIS_COUNT_2MOV_DEFAULT		5

unsigned long long getHysteresisCountToMoving(void);
int setHysteresisCountToMoving(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the hysteresis count to stationary plugin parameter */
#define PUD_GAT_HYSTERESIS_COUNT_2STAT_NAME			"gatewayHysteresisCountToStationary"

/** The default value of the hysteresis count to stationary plugin parameter */
#define PUD_GAT_HYSTERESIS_COUNT_2STAT_DEFAULT		17

unsigned long long getGatewayHysteresisCountToStationary(void);
int setGatewayHysteresisCountToStationary(const char *value, void *data,
		set_plugin_parameter_addon addon);

/** The name of the hysteresis count to moving plugin parameter */
#define PUD_GAT_HYSTERESIS_COUNT_2MOV_NAME			"GatewayHysteresisCountToMoving"

/** The default value of the hysteresis count to moving plugin parameter */
#define PUD_GAT_HYSTERESIS_COUNT_2MOV_DEFAULT		5

unsigned long long getGatewayHysteresisCountToMoving(void);
int setGatewayHysteresisCountToMoving(const char *value, void *data,
		set_plugin_parameter_addon addon);

/*
 * Other Plugin Settings
 */

/** The name of the deduplication usage plugin parameter */
#define PUD_USE_DEDUP_NAME						"useDeDup"

/** The default value of the deduplication usage plugin parameter */
#define PUD_USE_DEDUP_DEFAULT					true

bool getUseDeDup(void);
int
setUseDeDup(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the deduplication depth plugin parameter */
#define PUD_DEDUP_DEPTH_NAME					"deDupDepth"

/** The default value of the deduplication depth plugin parameter */
#define PUD_DEDUP_DEPTH_DEFAULT					256

unsigned long long getDeDupDepth(void);
int
setDeDupDepth(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the loopback usage plugin parameter */
#define PUD_USE_LOOPBACK_NAME					"useLoopback"

/** The default value of the loopback usage plugin parameter */
#define PUD_USE_LOOPBACK_DEFAULT				false

bool getUseLoopback(void);
int
setUseLoopback(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * gpsd
 */

/** The name of the gpsd update interval plugin parameter */
#define PUD_GPSD_USE_NAME  "gpsdUse"

/** The default value of the gpsdUse plugin parameter */
#define PUD_USE_GPSD_DEFAULT true

bool getGpsdUse(void);
int setGpsdUse(const char *value, void *data, set_plugin_parameter_addon addon);

/** The name of the gpsd plugin parameter */
#define PUD_GPSD_NAME  "gpsd"

GpsDaemon *getGpsd(void);
int setGpsd(const char *value, void *data, set_plugin_parameter_addon addon);

/*
 * Check Functions
 */

unsigned int checkConfig(void);

unsigned int checkRunSetup(void);

#endif /* _PUD_CONFIGURATION_H_ */
