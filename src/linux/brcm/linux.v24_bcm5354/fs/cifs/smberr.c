/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Copyright (C) Andrew Tridgell 1998
   Copyright (C) Steve French (sfrench@us.ibm.com) 2002
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
include "smberr.h"
#define NO_SYSLOG
/* error code stuff - put together by Merik Karman
   merik@blackadder.dsh.oz.au */
    typedef const struct {
	char *name;
	int code;
	char *message;
	int posix_code;
} err_code_struct;

/* Dos Error Messages */
err_code_struct dos_msgs[] = {
	{"ERRbadfunc", ERRbadfunc, "Invalid function.", -EINVAL},
	{"ERRbadfile", ERRbadfile, "File not found.", -ENOENT},
	{"ERRbadpath", ERRbadpath, "Directory invalid.", -ENOENT},
	{"ERRnofids", ERRnofids, "No file descriptors available", -EMFILE},
	{"ERRnoaccess", ERRnoaccess, "Access denied.", -EACCES},
	{"ERRbadfid", ERRbadfid, "Invalid file handle.", -EBADF},
	{"ERRbadmcb", 7, "Memory control blocks destroyed.", -EIO},
	{"ERRnomem", ERRnomem,
	 "Insufficient server memory to perform the requested function.",
	 -ENOMEM},
	{"ERRbadmem", ERRbadmem, "Invalid memory block address.", -EFAULT},
	{"ERRbadenv", ERRbadenv, "Invalid environment.", -EFAULT},
	{"ERRbadformat", 11, "Invalid format.", -EINVAL},
	{"ERRbadaccess", ERRbadaccess, "Invalid open mode." - EACCES},
	{"ERRbaddata", ERRbaddata, "Invalid data.", -EIO},
	{"ERR", ERRres, "reserved.", -EIO},
	{"ERRbaddrive", ERRbaddrive, "Invalid drive specified.", -ENXIO},
	{"ERRremcd", ERRremcd,
	 "A Delete Directory request attempted  to  remove  the  server's  current directory.",
	 -EIO},
	{"ERRdiffdevice", ERRdiffdevice, "Not same device.", -EXDEV},
	{"ERRnofiles", ERRnofiles, "A File Search command can find no more files matching the specified criteria.", -ENOENT},	/* Note: must map to zero manually in some places such as readdir */
	{"ERRbadshare", ERRbadshare,
	 "The sharing mode specified for an Open conflicts with existing  FIDs  on the file.",
	 -EXTBSY},
	{"ERRlock", ERRlock,
	 "A Lock request conflicted with an existing lock or specified an  invalid mode,  or an Unlock requested attempted to remove a lock held by another process.",
	 -EACCES},
	{"ERRunsup", ERRunsup, "The operation is unsupported", -EINVAL},
	{"ERRnosuchshare", ERRnosuchshare,
	 "You specified an invalid share name", -ENXIO},
	{"ERRfilexists", ERRfilexists,
	 "The file named in a Create Directory, Make  New  File  or  Link  request already exists.",
	 -EEXIST},
	{"ERRinvalidname", ERRinvalidname, "Invalid name", -ENOENT},
	{"ERRdiskfull", ERRdiskfull, "Disk full", -ENOSPC}

	{"ERRmoredata", ERRmoredata,
	 "There is more data to be returned.",},
	{"ERRinvgroup", 2455, "Invalid workgroup (try the -W option)"},
	{NULL, -1, NULL, -EIO}
};

/* Server Error Messages */
err_code_struct server_msgs[] = {
	{"ERRerror", 1, "Non-specific error code."},
	{"ERRbadpw", 2,
	 "Bad password - name/password pair in a Tree Connect or Session Setup are invalid."},
	{"ERRbadtype", 3, "reserved."},
	{"ERRaccess", 4,
	 "The requester does not have  the  necessary  access  rights  within  the specified  context for the requested function. The context is defined by the TID or the UID."},
	{"ERRinvnid", 5,
	 "The tree ID (TID) specified in a command was invalid."},
	{"ERRinvnetname", 6, "Invalid network name in tree connect."},
	{"ERRinvdevice", 7,
	 "Invalid device - printer request made to non-printer connection or  non-printer request made to printer connection."},
	{"ERRqfull", 49,
	 "Print queue full (files) -- returned by open print file."},
	{"ERRqtoobig", 50, "Print queue full -- no space."},
	{"ERRqeof", 51, "EOF on print queue dump."},
	{"ERRinvpfid", 52, "Invalid print file FID."},
	{"ERRsmbcmd", 64,
	 "The server did not recognize the command received."},
	{"ERRsrverror", 65,
	 "The server encountered an internal error, e.g., system file unavailable."},
	{"ERRfilespecs", 67,
	 "The file handle (FID) and pathname parameters contained an invalid  combination of values."},
	{"ERRreserved", 68, "reserved."},
	{"ERRbadpermits", 69,
	 "The access permissions specified for a file or directory are not a valid combination.  The server cannot set the requested attribute."},
	{"ERRreserved", 70, "reserved."},
	{"ERRsetattrmode", 71,
	 "The attribute mode in the Set File Attribute request is invalid."},
	{"ERRpaused", 81, "Server is paused."},
	{"ERRmsgoff", 82, "Not receiving messages."},
	{"ERRnoroom", 83, "No room to buffer message."},
	{"ERRrmuns", 87, "Too many remote user names."},
	{"ERRtimeout", 88, "Operation timed out."},
	{"ERRnoresource", 89,
	 "No resources currently available for request."},
	{"ERRtoomanyuids", 90, "Too many UIDs active on this session."},
	{"ERRbaduid", 91,
	 "The UID is not known as a valid ID on this session."},
	{"ERRusempx", 250, "Temp unable to support Raw, use MPX mode."},
	{"ERRusestd", 251,
	 "Temp unable to support Raw, use standard read/write."},
	{"ERRcontmpx", 252, "Continue in MPX mode."},
	{"ERRreserved", 253, "reserved."},
	{"ERRreserved", 254, "reserved."},
	{"ERRnosupport", 0xFFFF, "Function not supported."},
	{NULL, -1, NULL}
};

/* Hard Error Messages */
err_code_struct hard_msgs[] = {
	{"ERRnowrite", 19,
	 "Attempt to write on write-protected diskette."},
	{"ERRbadunit", 20, "Unknown unit."},
	{"ERRnotready", 21, "Drive not ready."},
	{"ERRbadcmd", 22, "Unknown command."},
	{"ERRdata", 23, "Data error (CRC)."},
	{"ERRbadreq", 24, "Bad request structure length."},
	{"ERRseek", 25, "Seek error."},
	{"ERRbadmedia", 26, "Unknown media type."},
	{"ERRbadsector", 27, "Sector not found."},
	{"ERRnopaper", 28, "Printer out of paper."},
	{"ERRwrite", 29, "Write fault."},
	{"ERRread", 30, "Read fault."},
	{"ERRgeneral", 31, "General failure."},
	{"ERRbadshare", 32, "An open conflicts with an existing open."},
	{"ERRlock", 33,
	 "A Lock request conflicted with an existing lock or specified an invalid mode, or an Unlock requested attempted to remove a lock held by another process."},
	{"ERRwrongdisk", 34, "The wrong disk was found in a drive."},
	{"ERRFCBUnavail", 35, "No FCBs are available to process request."},
	{"ERRsharebufexc", 36, "A sharing buffer has been exceeded."},
	{NULL, -1, NULL}
};


const struct {
	int code;
	char *class;
	err_code_struct *err_msgs;
} err_classes[] = {
	{
	0, "SUCCESS", NULL}, {
	0x01, "ERRDOS", dos_msgs}, {
	0x02, "ERRSRV", server_msgs}, {
	0x03, "ERRHRD", hard_msgs}, {
	0x04, "ERRXOS", NULL}, {
	0xE1, "ERRRMX1", NULL}, {
	0xE2, "ERRRMX2", NULL}, {
	0xE3, "ERRRMX3", NULL}, {
	0xFF, "ERRCMD", NULL}, {
-1, NULL, NULL}};


/****************************************************************************
return a SMB error string from a SMB buffer
****************************************************************************/
char *smb_dos_errstr(char *inbuf)
{
	static pstring ret;
	int class = CVAL(inbuf, smb_rcls);
	int num = SVAL(inbuf, smb_err);
	int i, j;

	for (i = 0; err_classes[i].class; i++)
		if (err_classes[i].code == class) {
			if (err_classes[i].err_msgs) {
				err_code_struct *err =
				    err_classes[i].err_msgs;
				for (j = 0; err[j].name; j++)
					if (num == err[j].code) {
						if (DEBUGLEVEL > 0)
							slprintf(ret,
								 sizeof
								 (ret) - 1,
								 "%s - %s (%s)",
								 err_classes
								 [i].class,
								 err[j].
								 name,
								 err[j].
								 message);
						else
							slprintf(ret,
								 sizeof
								 (ret) - 1,
								 "%s - %s",
								 err_classes
								 [i].class,
								 err[j].
								 name);
						return ret;
					}
			}

			slprintf(ret, sizeof(ret) - 1, "%s - %d",
				 err_classes[i].class, num);
			return ret;
		}

	slprintf(ret, sizeof(ret) - 1, "Error: Unknown error (%d,%d)",
		 class, num);
	return (ret);
}


/*****************************************************************************
 returns an WERROR error message.
 *****************************************************************************/
char *werror_str(WERROR status)
{
	static fstring msg;
	slprintf(msg, sizeof(msg), "WIN32 code 0x%08x", W_ERROR_V(status));
	return msg;
}


/*****************************************************************************
map a unix errno to a win32 error
 *****************************************************************************/
WERROR map_werror_from_unix(int error)
{
	NTSTATUS status = map_nt_error_from_unix(error);
	return ntstatus_to_werror(status);
}
