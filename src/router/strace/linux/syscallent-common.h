/*
 * Copyright (c) 2019 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef BASE_NR
# define BASE_NR 0
#endif
[BASE_NR + 424] = { 4,	TD|TS,		SEN(pidfd_send_signal),		"pidfd_send_signal"	},
[BASE_NR + 425] = { 2,	TD,		SEN(io_uring_setup),		"io_uring_setup"	},
[BASE_NR + 426] = { 6,	TD|TS,		SEN(io_uring_enter),		"io_uring_enter"	},
[BASE_NR + 427] = { 4,	TD|TM,		SEN(io_uring_register),		"io_uring_register"	},
[BASE_NR + 428] = { 3,	TD|TF,		SEN(open_tree),			"open_tree"		},
[BASE_NR + 429] = { 5,	TD|TF,		SEN(move_mount),		"move_mount"		},
[BASE_NR + 430] = { 2,	TD,		SEN(fsopen),			"fsopen"		},
[BASE_NR + 431] = { 5,	TD|TF,		SEN(fsconfig),			"fsconfig"		},
[BASE_NR + 432] = { 3,	TD,		SEN(fsmount),			"fsmount"		},
[BASE_NR + 433] = { 3,	TD|TF,		SEN(fspick),			"fspick"		},
[BASE_NR + 434] = { 2,	TD,		SEN(pidfd_open),		"pidfd_open"		},
[BASE_NR + 435] = { 2,	TP,		SEN(clone3),			"clone3"		},
