# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /cvsroot/openser/sip-server/packaging/gentoo/openser-1.0.0.ebuild,v 1.3 2005/10/27 18:57:56 bogdan_iancu Exp $

inherit eutils

DESCRIPTION="Open SIP Express Router"
HOMEPAGE="http://www.openser.org/"
MY_P="${P}_src"
SRC_URI="http://openser.org/pub/openser/${PV}/src/${MY_P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86"
IUSE="debug ipv6 mysql postgres radius jabber ssl"

RDEPEND="
	mysql? ( >=dev-db/mysql-3.23.52 )
	radius? ( >=net-dialup/radiusclient-ng-0.5.0 )
	postgres? ( >=dev-db/postgresql-7.3.4 )
	jabber? ( dev-libs/expat )
	ssl? ( dev-libs/openssl )"

inc_mod=""
make_options=""

pkg_setup() {
	use mysql && \
		inc_mod="${inc_mod} mysql"

	use postgres && \
		inc_mod="${inc_mod} postgres"

	use radius && \
		inc_mod="${inc_mod} auth_radius group_radius uri_radius avp_radius"

	use jabber && \
		inc_mod="${inc_mod} jabber"
		
	export inc_mod
}

src_unpack() {
	unpack ${MY_P}.tar.gz

	cd ${S}
	use ipv6 || \
		sed -i -e "s/-DUSE_IPV6//g" Makefile.defs
}

src_compile() {
	local compile_options

	pkg_setup

	# optimization can result in strange debuging symbols so omit it in case
	if use debug; then
		compile_options="${compile_options} mode=debug"
	else
		compile_options="${compile_options} CFLAGS=${CFLAGS}"
	fi
	
	if use ssl; then
		compile_options="TLS=1 ${compile_options}"
	fi

	emake all "${compile_options}" \
		prefix=${ROOT}/ \
		include_modules="${inc_mod}" \
		cfg-prefix=${ROOT}/ \
		cfg-target=${ROOT}/etc/openser/ || die
}

src_install () {
	local install_options

	emake install \
		prefix=${D}/ \
		include_modules="${inc_mod}" \
		bin-prefix=${D}/usr/sbin \
		bin-dir="" \
		cfg-prefix=${D}/etc \
		cfg-dir=openser/ \
		cfg-target=${D}/etc/openser \
		modules-prefix=${D}/usr/lib/openser \
		modules-dir=modules \
		modules-target=${D}/usr/lib/openser/modules/ \
		man-prefix=${D}/usr/share/man \
		man-dir="" \
		doc-prefix=${D}/usr/share/doc \
		doc-dir=${PF} || die
	exeinto /etc/init.d
	newexe ${FILESDIR}/openser.init openser

	# fix what the Makefile don't do
	use mysql || \
		rm ${D}/usr/sbin/openser_mysql.sh
}

pkg_postinst() {
	einfo "WARNING: If you upgraded from a previous Ser version"
	einfo "please read the README, NEWS and INSTALL files in the"
	einfo "documentation directory because the database and the"
	einfo "configuration file of old Ser versions are incompatible"
	einfo "with the current version."
}

pkg_prerm () {
	${D}/etc/init.d/openser stop >/dev/null
}
