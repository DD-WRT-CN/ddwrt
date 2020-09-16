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

#include "gpsConversion.h"

/* Plugin includes */
#include "pud.h"
#include "configuration.h"
#include "compiler.h"

/* OLSR includes */
#include "olsr.h"

/* System includes */
#include <nmealib/info.h>
#include <nmealib/nmath.h>
#include <nmealib/util.h>
#include <arpa/inet.h>
#include <OlsrdPudWireFormat/nodeIdConversion.h>

/* ************************************************************************
 * OLSR --> External
 * ************************************************************************ */

/**
 Convert an OLSR message into a string to multicast on the LAN

 @param olsrMessage
 A pointer to the OLSR message
 @param txGpsBuffer
 A pointer to the buffer in which the transmit string can be written
 @param txGpsBufferSize
 The size of the txGpsBuffer

 @return
 - the length of the transmit string placed in the txGpsBuffer
 - 0 (zero) in case of an error
 */
unsigned int gpsFromOlsr(union olsr_message *olsrMessage,
		unsigned char * txGpsBuffer, unsigned int txGpsBufferSize) {
	unsigned long validityTime;

	struct tm timeStruct;
	char latitudeString[PUD_TX_LATITUDE_DIGITS];
	const char * latitudeHemisphere;
	char longitudeString[PUD_TX_LONGITUDE_DIGITS];
	const char * longitudeHemisphere;
	char altitudeString[PUD_TX_ALTITUDE_DIGITS];
	char speedString[PUD_TX_SPEED_DIGITS];
	char trackString[PUD_TX_TRACK_DIGITS];
	char hdopString[PUD_TX_HDOP_DIGITS];
	uint32_t present;
	char gateway[2] = { '0', '\0' };

	char nodeIdTypeString[PUD_TX_NODEIDTYPE_DIGITS];
	char nodeIdString[PUD_TX_NODEID_BUFFERSIZE];
	const char * nodeId;
	const void * ipAddr;
	char originatorBuffer[64];
	const char * originator;

	unsigned int transmitStringLength;

	PudOlsrPositionUpdate * olsrGpsMessage =
			getOlsrMessagePayload(olsr_cnf->ip_version, olsrMessage);

	if (unlikely(getPositionUpdateVersion(olsrGpsMessage) != PUD_WIRE_FORMAT_VERSION)) {
		/* currently we can only handle our own version */
		pudError(false, "Can not handle version %u OLSR PUD messages"
			" (only version %u): message ignored",
			getPositionUpdateVersion(olsrGpsMessage), PUD_WIRE_FORMAT_VERSION);
		return 0;
	}

	ipAddr = (olsr_cnf->ip_version == AF_INET) ?
				(void *) &olsrMessage->v4.originator :
				(void *) &olsrMessage->v6.originator;
	originator = inet_ntop(olsr_cnf->ip_version, ipAddr, &originatorBuffer[0],
			sizeof(originatorBuffer));

	validityTime = getValidityTime(&olsrGpsMessage->validityTime);

	present = getPositionUpdatePresent(olsrGpsMessage);

	if (present & PUD_PRESENT_GATEWAY) {
		gateway[0] = '1';
	}

	/* time is ALWAYS present so we can just use it */
	getPositionUpdateTime(olsrGpsMessage, time(NULL), &timeStruct);

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_LAT))) {
		double latitude = getPositionUpdateLatitude(olsrGpsMessage);

		if (latitude >= 0) {
			latitudeHemisphere = "N";
		} else {
			latitudeHemisphere = "S";
			latitude = -latitude;
		}
		latitude = nmeaMathDegreeToNdeg(latitude);

		snprintf(&latitudeString[0], PUD_TX_LATITUDE_DIGITS, "%." PUD_TX_LATITUDE_DECIMALS "f", latitude);
	} else {
		latitudeHemisphere = "";
		latitudeString[0] = '\0';
	}

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_LON))) {
		double longitude = getPositionUpdateLongitude(olsrGpsMessage);

		if (longitude >= 0) {
			longitudeHemisphere = "E";
		} else {
			longitudeHemisphere = "W";
			longitude = -longitude;
		}
		longitude = nmeaMathDegreeToNdeg(longitude);

		snprintf(&longitudeString[0], PUD_TX_LONGITUDE_DIGITS, "%." PUD_TX_LONGITUDE_DECIMALS "f", longitude);
	} else {
		longitudeHemisphere = "";
		longitudeString[0] = '\0';
	}

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_ELV))) {
		snprintf(&altitudeString[0], PUD_TX_ALTITUDE_DIGITS, "%ld", getPositionUpdateAltitude(olsrGpsMessage));
	} else {
		altitudeString[0] = '\0';
	}

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_SPEED))) {
		snprintf(&speedString[0], PUD_TX_SPEED_DIGITS, "%lu", getPositionUpdateSpeed(olsrGpsMessage));
	} else {
		speedString[0] = '\0';
	}

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_TRACK))) {
		snprintf(&trackString[0], PUD_TX_TRACK_DIGITS, "%lu", getPositionUpdateTrack(olsrGpsMessage));
	} else {
		trackString[0] = '\0';
	}

	if (likely(nmeaInfoIsPresentAll(present, NMEALIB_PRESENT_HDOP))) {
		snprintf(&hdopString[0], PUD_TX_HDOP_DIGITS, "%." PUD_TX_HDOP_DECIMALS "f",
		    nmeaMathMetersToDop(getPositionUpdateHdop(olsrGpsMessage)));
	} else {
		hdopString[0] = '\0';
	}

	getNodeTypeStringFromOlsr(olsr_cnf->ip_version, olsrGpsMessage,
			&nodeIdTypeString[0], sizeof(nodeIdTypeString));
	getNodeIdStringFromOlsr(olsr_cnf->ip_version, olsrMessage, &nodeId,
			&nodeIdString[0], sizeof(nodeIdString));

	transmitStringLength = nmeaPrintf((char *) txGpsBuffer, txGpsBufferSize
			- 1, "$P%s," /* prefix (always) */
		"%u," /* sentence version (always) */
		"%s," /* gateway flag (always) */
		"%s," /* OLSR originator (always) */
		"%s,%s," /* nodeIdType/nodeId (always) */
		"%02u%02u%02u," /* date (always) */
		"%02u%02u%02u," /* time (always) */
		"%lu," /* validity time (always) */
		"%s,%s," /* latitude (optional) */
		"%s,%s," /* longitude (optional) */
		"%s," /* altitude (optional) */
		"%s," /* speed (optional) */
		"%s," /* track (optional) */
		"%s" /* hdop (optional) */
	, getTxNmeaMessagePrefix(), PUD_TX_SENTENCE_VERSION, &gateway[0],
			originator , &nodeIdTypeString[0],
			nodeId, timeStruct.tm_mday, timeStruct.tm_mon + 1, (timeStruct.tm_year
					% 100), timeStruct.tm_hour, timeStruct.tm_min,
			timeStruct.tm_sec, validityTime, &latitudeString[0],
			latitudeHemisphere, &longitudeString[0], longitudeHemisphere,
			&altitudeString[0], &speedString[0], &trackString[0],
			&hdopString[0]);

	if (unlikely(transmitStringLength > (txGpsBufferSize - 1))) {
		pudError(false, "String to transmit on non-OLSR is too large, need"
			" at least %u bytes, skipped", transmitStringLength);
		return 0;
	}

	if (unlikely(transmitStringLength == (txGpsBufferSize - 1))) {
		txGpsBuffer[txGpsBufferSize - 1] = '\0';
	} else {
		txGpsBuffer[transmitStringLength] = '\0';
	}

	return transmitStringLength;
}

/* ************************************************************************
 * External --> OLSR
 * ************************************************************************ */

/**
 Convert a nmeaINFO structure into an OLSR message.

 @param nmeaInfo
 A pointer to a nmeaINFO structure
 @param olsrMessage
 A pointer to an OLSR message in which to place the converted information
 @param olsrMessageSize
 The maximum number of bytes available for the olsrMessage
 @param validityTime
 the validity time of the message in seconds

 @return
 - the aligned size of the converted information
 - 0 (zero) in case of an error
 */
unsigned int gpsToOlsr(NmeaInfo *nmeaInfo, union olsr_message *olsrMessage,
		unsigned int olsrMessageSize, unsigned long long validityTime) {
	unsigned int aligned_size;
	unsigned int aligned_size_remainder;
	size_t nodeLength;
	nodeIdBinaryType * nodeIdBinary = NULL;

	PudOlsrPositionUpdate * olsrGpsMessage =
			getOlsrMessagePayload(olsr_cnf->ip_version, olsrMessage);

	/*
	 * Compose message contents
	 */
	memset(olsrGpsMessage, 0, sizeof (PudOlsrPositionUpdate));

	setPositionUpdateVersion(olsrGpsMessage, PUD_WIRE_FORMAT_VERSION);
	setValidityTime(&olsrGpsMessage->validityTime, validityTime);
	setPositionUpdatePresent(olsrGpsMessage, nmeaInfo->present & ~PUD_PRESENT_GATEWAY);

	/* utc is always present, we make sure of that elsewhere, so just use it */
	setPositionUpdateTime(olsrGpsMessage, nmeaInfo->utc.hour, nmeaInfo->utc.min,
			nmeaInfo->utc.sec);

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_LAT))) {
		setPositionUpdateLatitude(olsrGpsMessage, nmeaInfo->latitude);
	} else {
		setPositionUpdateLatitude(olsrGpsMessage, 0.0);
	}

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_LON))) {
		setPositionUpdateLongitude(olsrGpsMessage, nmeaInfo->longitude);
	} else {
		setPositionUpdateLongitude(olsrGpsMessage, 0.0);
	}

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_ELV))) {
		setPositionUpdateAltitude(olsrGpsMessage, nmeaInfo->elevation);
	} else {
		setPositionUpdateAltitude(olsrGpsMessage, 0.0);
	}

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_SPEED))) {
		setPositionUpdateSpeed(olsrGpsMessage, nmeaInfo->speed);
	} else {
		setPositionUpdateSpeed(olsrGpsMessage, 0.0);
	}

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_TRACK))) {
		setPositionUpdateTrack(olsrGpsMessage, nmeaInfo->track);
	} else {
		setPositionUpdateTrack(olsrGpsMessage, 0);
	}

	if (likely(nmeaInfoIsPresentAll(nmeaInfo->present, NMEALIB_PRESENT_HDOP))) {
		setPositionUpdateHdop(olsrGpsMessage, nmeaInfo->hdop);
	} else {
		setPositionUpdateHdop(olsrGpsMessage, PUD_HDOP_MAX);
	}

	nodeIdBinary = getNodeIdBinary();
	nodeLength = setPositionUpdateNodeInfo(olsr_cnf->ip_version, olsrGpsMessage,
			olsrMessageSize, getNodeIdTypeNumber(),
			(unsigned char *) &nodeIdBinary->buffer, nodeIdBinary->length);

	/*
	 * Messages in OLSR are 4-byte aligned: align
	 */

	/* size = type, string, string terminator */
	aligned_size = PUD_OLSRWIREFORMATSIZE + nodeLength;
	aligned_size_remainder = (aligned_size % 4);
	if (aligned_size_remainder != 0) {
		aligned_size += (4 - aligned_size_remainder);
	}

	/*
	 * Fill message headers (fill ALL fields, except message)
	 * Note: olsr_vtime is currently unused, we use it for our validity time.
	 */

	if (olsr_cnf->ip_version == AF_INET) {
		/* IPv4 */

		olsrMessage->v4.olsr_msgtype = PUD_OLSR_MSG_TYPE;
		olsrMessage->v4.olsr_vtime = reltime_to_me(validityTime * 1000);
		/* message->v4.olsr_msgsize at the end */
		olsrMessage->v4.originator = olsr_cnf->main_addr.v4.s_addr;
		olsrMessage->v4.ttl = getOlsrTtl();
		olsrMessage->v4.hopcnt = 0;
		olsrMessage->v4.seqno = htons(get_msg_seqno());

		/* add length of message->v4 fields */
		aligned_size += (sizeof(olsrMessage->v4)
				- sizeof(olsrMessage->v4.message));
		olsrMessage->v4.olsr_msgsize = htons(aligned_size);
	} else {
		/* IPv6 */

		olsrMessage->v6.olsr_msgtype = PUD_OLSR_MSG_TYPE;
		olsrMessage->v6.olsr_vtime = reltime_to_me(validityTime * 1000);
		/* message->v6.olsr_msgsize at the end */
		olsrMessage->v6.originator = olsr_cnf->main_addr.v6;
		olsrMessage->v6.ttl = getOlsrTtl();
		olsrMessage->v6.hopcnt = 0;
		olsrMessage->v6.seqno = htons(get_msg_seqno());

		/* add length of message->v6 fields */
		aligned_size += (sizeof(olsrMessage->v6)
				- sizeof(olsrMessage->v6.message));
		olsrMessage->v6.olsr_msgsize = htons(aligned_size);
	}

	/* pad with zeroes */
	if (aligned_size_remainder != 0) {
		memset(&(((char *) &olsrGpsMessage->nodeInfo.nodeIdType)[nodeLength]),
				0, (4 - aligned_size_remainder));
	}

	return aligned_size;
}
