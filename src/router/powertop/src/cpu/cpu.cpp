/*
 * Copyright 2010, Intel Corporation
 *
 * This file is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 * or just google for it.
 *
 * Authors:
 *	Arjan van de Ven <arjan@linux.intel.com>
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include "cpu.h"
#include "cpudevice.h"
#include "cpu_rapl_device.h"
#include "dram_rapl_device.h"
#include "intel_cpus.h"
#include "../parameters/parameters.h"

#include "../perf/perf_bundle.h"
#include "../lib.h"
#include "../display.h"
#include "../report/report.h"
#include "../report/report-maker.h"
#include "../report/report-data-html.h"

static class abstract_cpu system_level;

vector<class abstract_cpu *> all_cpus;

static	class perf_bundle * perf_events;



class perf_power_bundle: public perf_bundle
{
	virtual void handle_trace_point(void *trace, int cpu, uint64_t time);

};


static class abstract_cpu * new_package(int package, int cpu, char * vendor, int family, int model)
{
	class abstract_cpu *ret = NULL;
	class cpudevice *cpudev;
	class cpu_rapl_device *cpu_rapl_dev;
	class dram_rapl_device *dram_rapl_dev;

	char packagename[128];
	if (strcmp(vendor, "GenuineIntel") == 0)
		if (family == 6)
			if (is_supported_intel_cpu(model, cpu)) {
				ret = new class nhm_package(model);
				ret->set_intel_MSR(true);
			}

	if (!ret) {
		ret = new class cpu_package;
		ret->set_intel_MSR(false);
	}

	ret->set_number(package, cpu);
	ret->set_type("Package");
	ret->childcount = 0;

	snprintf(packagename, sizeof(packagename), _("cpu package %i"), cpu);
	cpudev = new class cpudevice(_("cpu package"), packagename, ret);
	all_devices.push_back(cpudev);

	snprintf(packagename, sizeof(packagename), _("package-%i"), cpu);
	cpu_rapl_dev = new class cpu_rapl_device(cpudev, _("cpu rapl package"), packagename, ret);
	if (cpu_rapl_dev->device_present())
		all_devices.push_back(cpu_rapl_dev);
	else
		delete cpu_rapl_dev;

	snprintf(packagename, sizeof(packagename), _("package-%i"), cpu);
	dram_rapl_dev = new class dram_rapl_device(cpudev, _("dram rapl package"), packagename, ret);
	if (dram_rapl_dev->device_present())
		all_devices.push_back(dram_rapl_dev);
	else
		delete dram_rapl_dev;

	return ret;
}

static class abstract_cpu * new_core(int core, int cpu, char * vendor, int family, int model)
{
	class abstract_cpu *ret = NULL;

	if (strcmp(vendor, "GenuineIntel") == 0)
		if (family == 6)
			if (is_supported_intel_cpu(model, cpu)) {
				ret = new class nhm_core(model);
				ret->set_intel_MSR(true);
			}

	if (!ret) {
		ret = new class cpu_core;
		ret->set_intel_MSR(false);
	}

	ret->set_number(core, cpu);
	ret->childcount = 0;
	ret->set_type("Core");

	return ret;
}

static class abstract_cpu * new_i965_gpu(void)
{
	class abstract_cpu *ret = NULL;

	ret = new class i965_core;
	ret->childcount = 0;
	ret->set_type("GPU");

	return ret;
}

static class abstract_cpu * new_cpu(int number, char * vendor, int family, int model)
{
	class abstract_cpu * ret = NULL;

	if (strcmp(vendor, "GenuineIntel") == 0)
		if (family == 6)
			if (is_supported_intel_cpu(model, number)) {
				ret = new class nhm_cpu;
				ret->set_intel_MSR(true);
			}

	if (!ret) {
		ret = new class cpu_linux;
		ret->set_intel_MSR(false);
	}
	ret->set_number(number, number);
	ret->set_type("CPU");
	ret->childcount = 0;

	return ret;
}




static void handle_one_cpu(unsigned int number, char *vendor, int family, int model)
{
	char filename[PATH_MAX];
	ifstream file;
	unsigned int package_number = 0;
	unsigned int core_number = 0;
	class abstract_cpu *package, *core, *cpu;

	snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpu%i/topology/core_id", number);
	file.open(filename, ios::in);
	if (file) {
		file >> core_number;
		file.close();
	}

	snprintf(filename, sizeof(filename), "/sys/devices/system/cpu/cpu%i/topology/physical_package_id", number);
	file.open(filename, ios::in);
	if (file) {
		file >> package_number;
		if (package_number == (unsigned int) -1)
			package_number = 0;
		file.close();
	}


	if (system_level.children.size() <= package_number)
		system_level.children.resize(package_number + 1, NULL);

	if (!system_level.children[package_number]) {
		system_level.children[package_number] = new_package(package_number, number, vendor, family, model);
		system_level.childcount++;
	}

	package = system_level.children[package_number];
	package->parent = &system_level;

	if (package->children.size() <= core_number)
		package->children.resize(core_number + 1, NULL);

	if (!package->children[core_number]) {
		package->children[core_number] = new_core(core_number, number, vendor, family, model);
		package->childcount++;
	}

	core = package->children[core_number];
	core->parent = package;

	if (core->children.size() <= number)
		core->children.resize(number + 1, NULL);
	if (!core->children[number]) {
		core->children[number] = new_cpu(number, vendor, family, model);
		core->childcount++;
	}

	cpu = core->children[number];
	cpu->parent = core;

	if (number >= all_cpus.size())
		all_cpus.resize(number + 1, NULL);
	all_cpus[number] = cpu;
}

static void handle_i965_gpu(void)
{
	unsigned int core_number = 0;
	class abstract_cpu *package;


	package = system_level.children[0];

	core_number = package->children.size();

	if (package->children.size() <= core_number)
		package->children.resize(core_number + 1, NULL);

	if (!package->children[core_number]) {
		package->children[core_number] = new_i965_gpu();
		package->childcount++;
	}
}


void enumerate_cpus(void)
{
	ifstream file;
	char line[1024];

	int number = -1;
	char vendor[128];
	int family = 0;
	int model = 0;

	file.open("/proc/cpuinfo",  ios::in);

	if (!file)
		return;
	/* Not all /proc/cpuinfo include "vendor_id\t". */
	vendor[0] = '\0';

	while (file) {

		file.getline(line, sizeof(line));
		if (strncmp(line, "vendor_id\t",10) == 0) {
			char *c;
			c = strchr(line, ':');
			if (c) {
				c++;
				if (*c == ' ')
					c++;
				pt_strcpy(vendor, c);
			}
		}
		if (strncmp(line, "processor\t",10) == 0) {
			char *c;
			c = strchr(line, ':');
			if (c) {
				c++;
				number = strtoull(c, NULL, 10);
			}
		}
		if (strncmp(line, "cpu family\t",11) == 0) {
			char *c;
			c = strchr(line, ':');
			if (c) {
				c++;
				family = strtoull(c, NULL, 10);
			}
		}
		if (strncmp(line, "model\t",6) == 0) {
			char *c;
			c = strchr(line, ':');
			if (c) {
				c++;
				model = strtoull(c, NULL, 10);
			}
		}
		/* on x86 and others 'bogomips' is last
		 * on ARM it *can* be bogomips, or 'CPU revision'
		 * on POWER, it's revision
		 */
		if (strncasecmp(line, "bogomips\t", 9) == 0
		    || strncasecmp(line, "CPU revision\t", 13) == 0
		    || strncmp(line, "revision", 7) == 0) {
			if (number == -1) {
				/* Not all /proc/cpuinfo include "processor\t". */
				number = 0;
			}
			if (number >= 0) {
				handle_one_cpu(number, vendor, family, model);
				set_max_cpu(number);
				number = -2;
			}
		}
	}


	file.close();

	if (access("/sys/class/drm/card0/power/rc6_residency_ms", R_OK) == 0)
		handle_i965_gpu();

	perf_events = new perf_power_bundle();

	if (!perf_events->add_event("power:cpu_idle")){
		perf_events->add_event("power:power_start");
		perf_events->add_event("power:power_end");
	}
	if (!perf_events->add_event("power:cpu_frequency"))
		perf_events->add_event("power:power_frequency");

}

void start_cpu_measurement(void)
{
	perf_events->start();
	system_level.measurement_start();
}

void end_cpu_measurement(void)
{
	system_level.measurement_end();
	perf_events->stop();
}

static void expand_string(char *string, unsigned int newlen)
{
	while (strlen(string) < newlen)
		strcat(string, " ");
}

static int has_state_level(class abstract_cpu *acpu, int state, int line)
{
	switch (state) {
		case PSTATE:
			return acpu->has_pstate_level(line);
			break;
		case CSTATE:
			return acpu->has_cstate_level(line);
			break;
	}
	return 0;
}

static const char * fill_state_name(class abstract_cpu *acpu, int state, int line, char *buf)
{
	switch (state) {
		case PSTATE:
			return acpu->fill_pstate_name(line, buf);
			break;
		case CSTATE:
			return acpu->fill_cstate_name(line, buf);
			break;
	}
	return "-EINVAL";
}

static const char * fill_state_line(class abstract_cpu *acpu, int state, int line,
					char *buf, const char *sep = "")
{
	switch (state) {
		case PSTATE:
			return acpu->fill_pstate_line(line, buf);
			break;
		case CSTATE:
			return acpu->fill_cstate_line(line, buf, sep);
			break;
	}
	return "-EINVAL";
}

static int get_cstates_num(void)
{
	unsigned int package, core, cpu;
	class abstract_cpu *_package, * _core, * _cpu;
	unsigned int i;
	int cstates_num;

	for (package = 0, cstates_num = 0;
			package < system_level.children.size(); package++) {
		_package = system_level.children[package];
		if (_package == NULL)
			continue;

		/* walk package cstates and get largest cstates number */
		for (i = 0; i < _package->cstates.size(); i++)
			cstates_num = std::max(cstates_num,
						(_package->cstates[i])->line_level);

		/*
		 * for each core in this package, walk core cstates and get
		 * largest cstates number
		 */
		for (core = 0; core < _package->children.size(); core++) {
			_core = _package->children[core];
			if (_core == NULL)
				continue;

			for (i = 0; i <  _core->cstates.size(); i++)
				cstates_num = std::max(cstates_num,
						(_core->cstates[i])->line_level);

			/*
			 * for each core, walk the logical cpus in case
			 * there is are more linux cstates than hw cstates
			 */
			 for (cpu = 0; cpu < _core->children.size(); cpu++) {
				_cpu = _core->children[cpu];
				if (_cpu == NULL)
					continue;

				for (i = 0; i < _cpu->cstates.size(); i++)
					cstates_num = std::max(cstates_num,
						(_cpu->cstates[i])->line_level);
			}
		}
	}

	return cstates_num;
}

void report_display_cpu_cstates(void)
{
	char buffer[512], buffer2[512], tmp_num[50];
	unsigned int package, core, cpu;
	int line, cstates_num, title=0, core_num=0;
	class abstract_cpu *_package, *_core = NULL, * _cpu;
	const char* core_type = NULL;

	cstates_num = get_cstates_num();
	/* div attr css_class and css_id */
	tag_attr div_attr;
	init_div(&div_attr, "clear_block", "cpuidle");

	/* Set Table attributes, rows, and cols */
	table_attributes std_table_css;
	table_size pkg_tbl_size;
	table_size core_tbl_size;
	table_size cpu_tbl_size;


	/* Set Title attributes */
	tag_attr title_attr;
	init_title_attr(&title_attr);

	/* Report add section */
	report.add_div(&div_attr);
	report.add_title(&title_attr, __("Processor Idle State Report"));

	/* Set array of data in row Major order */
       	int idx1, idx2, idx3;
	string tmp_str;

	for (package = 0; package < system_level.children.size(); package++) {
		bool first_core = true;
		idx1=0;
		idx2=0;
		idx3=0;

		_package = system_level.children[package];
		if (!_package)
			continue;
		/* Tables for PKG, CORE, CPU */
		pkg_tbl_size.cols=2;
		pkg_tbl_size.rows= ((cstates_num+1)-LEVEL_HEADER)+1;
		string *pkg_data = new string[pkg_tbl_size.cols * pkg_tbl_size.rows];

		core_tbl_size.cols=2;
		core_tbl_size.rows=(cstates_num *_package->children.size())
				+ _package->children.size();
		string *core_data = new string[core_tbl_size.cols * core_tbl_size.rows];
		int num_cpus=0, num_cores=0;

		for (core = 0; core < _package->children.size(); core++) {
			_core = _package->children[core];
			if (!_core)
				continue;
			core_type = _core->get_type();
			if (core_type != NULL)
				if (strcmp(core_type, "Core") == 0 )
					num_cores+=1;

			for (cpu = 0; cpu < _core->children.size(); cpu++) {
				_cpu = _core->children[cpu];
				if (!_cpu)
					continue;
				num_cpus+=1;
			}
		}
		cpu_tbl_size.cols=(2 * (num_cpus / num_cores)) + 1;
		cpu_tbl_size.rows = ((cstates_num+1-LEVEL_HEADER) * _package->children.size())
				+ _package->children.size();
		string *cpu_data = new string[cpu_tbl_size.cols * cpu_tbl_size.rows];

		for (core = 0; core < _package->children.size(); core++) {
			cpu_data[idx3]="&nbsp;";
			idx3+=1;
			_core = _package->children[core];

			if (!_core)
				continue;

			/* *** PKG STARTS *** */
			for (line = LEVEL_HEADER; line <= cstates_num; line++) {
				bool first_cpu = true;
				if (!_package->has_cstate_level(line))
				continue;
				buffer[0] = 0;
				buffer2[0] = 0;
				if (line == LEVEL_HEADER) {
					if (first_core) {
						pkg_data[idx1]=__("Package");
						idx1+=1;
						sprintf(tmp_num,"%d",  _package->get_number());
						pkg_data[idx1]= string(tmp_num);
						idx1+=1;
					}
				} else if (first_core) {
					tmp_str=string(_package->fill_cstate_name(line, buffer));
					pkg_data[idx1]=(tmp_str=="" ? "&nbsp;" : tmp_str);
					idx1+=1;
					tmp_str=string(_package->fill_cstate_line(line, buffer2));
					pkg_data[idx1]=(tmp_str=="" ? "&nbsp;" : tmp_str);
					idx1+=1;
				}

				/* *** CORE STARTS *** */
				if (!_core->can_collapse()) {
					buffer[0] = 0;
					buffer2[0] = 0;

					if (line == LEVEL_HEADER) {
						/* Here we need to check for which core type we
						* are using. Do not use the core type for the
						* report.addf as it breaks an important macro use
						* for translation decision making for the reports.
						* */
						core_type = _core->get_type();
						if (core_type != NULL) {
							if (strcmp(core_type, "Core") == 0 ) {
								core_data[idx2]="";
								idx2+=1;
								snprintf(tmp_num, sizeof(tmp_num), __("Core %d"), _core->get_number());
								core_data[idx2]=string(tmp_num);
								idx2+=1;
								core_num+=1;
							} else {
								core_data[idx2]="";
								idx2+=1;
								snprintf(tmp_num, sizeof(tmp_num), __("GPU %d"), _core->get_number());
								core_data[idx2]=string(tmp_num);
								idx2+=1;
							}
						}
					} else {
						tmp_str=string(_core->fill_cstate_name(line, buffer));
						core_data[idx2]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx2+=1;
						tmp_str=string(_core->fill_cstate_line(line, buffer2));
						core_data[idx2]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx2+=1;
					}
				}
				// *** CPU STARTS ***
				for (cpu = 0; cpu < _core->children.size(); cpu++) {
					_cpu = _core->children[cpu];

					if (!_cpu)
						continue;
					if (line == LEVEL_HEADER) {
						cpu_data[idx3] = __("CPU");
						idx3+=1;
						sprintf(tmp_num,"%d",_cpu->get_number());
						cpu_data[idx3]=string(tmp_num);
						idx3+=1;
						continue;
					}

					if (first_cpu) {
						title+=1;
						cpu_data[idx3]=(string(_cpu->fill_cstate_name(line, buffer)));
						idx3+=1;
						first_cpu = false;
					}

					buffer[0] = 0;
					tmp_str=string(_cpu->fill_cstate_percentage(line, buffer));
					cpu_data[idx3]=(tmp_str=="" ? "&nbsp;" : tmp_str);
					idx3+=1;

					if (line != LEVEL_C0){
						tmp_str=string(_cpu->fill_cstate_time(line, buffer));
						cpu_data[idx3]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx3+=1;
					} else {
						cpu_data[idx3]="&nbsp;";
						idx3+=1;
					}
				}
			}
			first_core = false;
		}

		/* Report Output */
		if(core_num > 0)
			title=title/core_num;
		else if(_core && _core->children.size() > 0)
			title=title/_core->children.size();

		init_pkg_table_attr(&std_table_css, pkg_tbl_size.rows,  pkg_tbl_size.cols);
		report.add_table(pkg_data, &std_table_css);
		if (!_core->can_collapse()){
			init_core_table_attr(&std_table_css, title+1, core_tbl_size.rows,
				core_tbl_size.cols);
			report.add_table(core_data, &std_table_css);
		}
		init_cpu_table_attr(&std_table_css, title+1, cpu_tbl_size.rows,
				cpu_tbl_size.cols);
		report.add_table(cpu_data, &std_table_css);
		delete [] pkg_data;
		delete [] core_data;
		delete [] cpu_data;
	}
	report.end_div();
}

void report_display_cpu_pstates(void)
{
	char buffer[512], buffer2[512], tmp_num[50];
	unsigned int package, core, cpu;
	int line, title=0;
	class abstract_cpu *_package, *_core = NULL, * _cpu;
	unsigned int i, pstates_num;
	const char* core_type = NULL;

	/* div attr css_class and css_id */
	tag_attr div_attr;
	init_div(&div_attr, "clear_block", "cpufreq");

	/* Set Table attributes, rows, and cols */
	table_attributes std_table_css;
	table_size pkg_tbl_size;
	table_size core_tbl_size;
	table_size cpu_tbl_size;


	/* Set Title attributes */
	tag_attr title_attr;
	init_title_attr(&title_attr);

	/* Report add section */
        report.add_div(&div_attr);
        report.add_title(&title_attr, __("Processor Frequency Report"));

        /* Set array of data in row Major order */
	int idx1, idx2, idx3, num_cpus=0, num_cores=0;
        string tmp_str;

	for (i = 0, pstates_num = 0; i < all_cpus.size(); i++) {
		if (all_cpus[i])
			pstates_num = std::max<unsigned int>(pstates_num,
				all_cpus[i]->pstates.size());
	}

	for (package = 0; package < system_level.children.size(); package++) {
		bool first_core = true;
		idx1=0;
		idx2=0;
		idx3=0;

		_package = system_level.children[package];
		if (!_package)
			continue;

		/*  Tables for PKG, CORE, CPU */
		pkg_tbl_size.cols=2;
		pkg_tbl_size.rows=((pstates_num+1)-LEVEL_HEADER)+2;
		string *pkg_data = new string[pkg_tbl_size.cols * pkg_tbl_size.rows];

		core_tbl_size.cols=2;
		core_tbl_size.rows=((pstates_num+2) *_package->children.size());
		string *core_data = new string[core_tbl_size.cols * core_tbl_size.rows];

		/* PKG */
		num_cpus=0;
		num_cores=0;
		for (core = 0; core < _package->children.size(); core++) {
			_core = _package->children[core];
			if (!_core)
				continue;

			core_type = _core->get_type();
			if (core_type != NULL)
				if (strcmp(core_type, "Core") == 0 )
					num_cores+=1;

			for (cpu = 0; cpu < _core->children.size(); cpu++) {
				_cpu = _core->children[cpu];
				if (!_cpu)
					continue;
				num_cpus+=1;
			}
		}
		cpu_tbl_size.cols= (num_cpus/ num_cores) + 1;
		cpu_tbl_size.rows= (pstates_num+2) * _package->children.size()
				+ _package->children.size();
		string *cpu_data = new string[cpu_tbl_size.cols * cpu_tbl_size.rows];

		/* Core */
		for (core = 0; core < _package->children.size(); core++) {
			cpu_data[idx3]="&nbsp;";
			idx3+=1;
			_core = _package->children[core];
			if (!_core)
				continue;

			if (!_core->has_pstates())
				continue;

			for (line = LEVEL_HEADER; line < (int)pstates_num; line++) {
				bool first_cpu = true;

				if (!_package->has_pstate_level(line))
					continue;

				buffer[0] = 0;
				buffer2[0] = 0;
				if (first_core) {
					if (line == LEVEL_HEADER) {
						pkg_data[idx1]=__("Package");
						idx1+=1;
						sprintf(tmp_num,"%d",  _package->get_number());
						pkg_data[idx1]= string(tmp_num);
						idx1+=1;
					} else {
						tmp_str=string(_package->fill_pstate_name(line, buffer));
						pkg_data[idx1]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx1+=1;
						tmp_str=string(_package->fill_pstate_line(line, buffer2));
						pkg_data[idx1]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx1+=1;
					}
				}


				if (!_core->can_collapse()) {
					buffer[0] = 0;
					buffer2[0] = 0;
					if (line == LEVEL_HEADER) {
						core_data[idx2]="";
						idx2+=1;
						snprintf(tmp_num, sizeof(tmp_num), __("Core %d"), _core->get_number());
						core_data[idx2]=string(tmp_num);
						idx2+=1;
					} else {
						tmp_str=string(_core->fill_pstate_name(line, buffer));
						core_data[idx2]= (tmp_str=="" ? "&nbsp;" : tmp_str);
						idx2+=1;
						tmp_str=string(_core->fill_pstate_line(line, buffer2));
						core_data[idx2]= (tmp_str=="" ? "&nbsp;" : tmp_str);
						idx2+=1;
					}
				}

				/* CPU */
				for (cpu = 0; cpu < _core->children.size(); cpu++) {
					buffer[0] = 0;
					_cpu = _core->children[cpu];
					if (!_cpu)
						continue;

					if (line == LEVEL_HEADER) {
						snprintf(tmp_num, sizeof(tmp_num), __("CPU %d"), _cpu->get_number());
						cpu_data[idx3] = string(tmp_num);
						idx3+=1;
						continue;
					}

					if (first_cpu) {
						tmp_str=string(_cpu->fill_pstate_name(line, buffer));
						cpu_data[idx3]=(tmp_str=="" ? "&nbsp;" : tmp_str);
						idx3+=1;
						first_cpu = false;
					}

					buffer[0] = 0;
					tmp_str=string(_cpu->fill_pstate_line(line, buffer));
					cpu_data[idx3]=(tmp_str=="" ? "&nbsp;" : tmp_str);
					idx3+=1;
				}
			}
			first_core = false;
		}
		init_pkg_table_attr(&std_table_css, pkg_tbl_size.rows, pkg_tbl_size.cols);
		report.add_table(pkg_data, &std_table_css);
		if(_core && !_core->can_collapse()){
			title=pstates_num+2;
			init_core_table_attr(&std_table_css, title,
				core_tbl_size.rows, core_tbl_size.cols);
			report.add_table(core_data, &std_table_css);
		} else {
			title=pstates_num+1;
		}

		init_cpu_table_attr(&std_table_css, title,
				cpu_tbl_size.rows, cpu_tbl_size.cols);
		report.add_table(cpu_data, &std_table_css);
		delete [] pkg_data;
		delete [] core_data;
		delete [] cpu_data;
	}
	report.end_div();
}

void impl_w_display_cpu_states(int state)
{
	WINDOW *win;
	char buffer[128];
	char linebuf[1024];
	unsigned int package, core, cpu;
	int line, loop, cstates_num, pstates_num;
	class abstract_cpu *_package, * _core, * _cpu;
	int ctr = 0;
	unsigned int i;

	cstates_num = get_cstates_num();

	for (i = 0, pstates_num = 0; i < all_cpus.size(); i++) {
		if (!all_cpus[i])
			continue;

		pstates_num = std::max<int>(pstates_num, all_cpus[i]->pstates.size());
	}

	if (state == PSTATE) {
		win = get_ncurses_win("Frequency stats");
		loop = pstates_num;
	} else {
		win = get_ncurses_win("Idle stats");
		loop = cstates_num;
	}

	if (!win)
		return;

	wclear(win);
        wmove(win, 2,0);

	for (package = 0; package < system_level.children.size(); package++) {
		int first_pkg = 0;
		_package = system_level.children[package];
		if (!_package)
			continue;

		for (core = 0; core < _package->children.size(); core++) {
			_core = _package->children[core];
			if (!_core)
				continue;
			if (!_core->has_pstates() && state == PSTATE)
				continue;

			for (line = LEVEL_HEADER; line <= loop; line++) {
				int first = 1;
				ctr = 0;
				linebuf[0] = 0;

				if (!has_state_level(_package, state, line))
					continue;

				buffer[0] = 0;
				if (first_pkg == 0) {
					strcat(linebuf, fill_state_name(_package, state, line, buffer));
					expand_string(linebuf, ctr + 10);
					strcat(linebuf, fill_state_line(_package, state, line, buffer));
				}
				ctr += 20;
				expand_string(linebuf, ctr);

				strcat(linebuf, "| ");
				ctr += strlen("| ");

				if (!_core->can_collapse()) {
					buffer[0] = 0;
					strcat(linebuf, fill_state_name(_core, state, line, buffer));
					expand_string(linebuf, ctr + 10);
					strcat(linebuf, fill_state_line(_core, state, line, buffer));
					ctr += 20;
					expand_string(linebuf, ctr);

					strcat(linebuf, "| ");
					ctr += strlen("| ");
				}

				for (cpu = 0; cpu < _core->children.size(); cpu++) {
					_cpu = _core->children[cpu];
					if (!_cpu)
						continue;

					if (first == 1) {
						strcat(linebuf, fill_state_name(_cpu, state, line, buffer));
						expand_string(linebuf, ctr + 10);
						first = 0;
						ctr += 12;
					}
					buffer[0] = 0;
					strcat(linebuf, fill_state_line(_cpu, state, line, buffer));
					ctr += 10;
					expand_string(linebuf, ctr);

				}
				strcat(linebuf, "\n");
				wprintw(win, "%s", linebuf);
			}
			wprintw(win, "\n");
			first_pkg++;
		}
	}
}

void w_display_cpu_pstates(void)
{
	impl_w_display_cpu_states(PSTATE);
}

void w_display_cpu_cstates(void)
{
	impl_w_display_cpu_states(CSTATE);
}

struct power_entry {
#ifndef __i386__
	int dummy;
#endif
	int64_t	type;
	int64_t	value;
} __attribute__((packed));


void perf_power_bundle::handle_trace_point(void *trace, int cpunr, uint64_t time)
{
	struct event_format *event;
        struct pevent_record rec; /* holder */
	class abstract_cpu *cpu;
	int type;

	rec.data = trace;

	type = pevent_data_type(perf_event::pevent, &rec);
	event = pevent_find_event(perf_event::pevent, type);

	if (!event)
		return;

	if (cpunr >= (int)all_cpus.size()) {
		cout << "INVALID cpu nr in handle_trace_point\n";
		return;
	}

	cpu = all_cpus[cpunr];

#if 0
	unsigned int i;
	printf("Time is %llu \n", time);
	for (i = 0; i < system_level.children.size(); i++)
		if (system_level.children[i])
			system_level.children[i]->validate();
#endif
	unsigned long long val;
	int ret;
	if (strcmp(event->name, "cpu_idle")==0) {

		ret = pevent_get_field_val(NULL, event, "state", &rec, &val, 0);
                if (ret < 0) {
                        fprintf(stderr, _("cpu_idle event returned no state?\n"));
                        exit(-1);
                }

		if (val == (unsigned int)-1)
			cpu->go_unidle(time);
		else
			cpu->go_idle(time);
	}

	if (strcmp(event->name, "power_frequency") == 0
	|| strcmp(event->name, "cpu_frequency") == 0){

		ret = pevent_get_field_val(NULL, event, "state", &rec, &val, 0);
		if (ret < 0) {
			fprintf(stderr, _("power or cpu_frequency event returned no state?\n"));
			exit(-1);
		}

		cpu->change_freq(time, val);
	}

	if (strcmp(event->name, "power_start")==0)
		cpu->go_idle(time);
	if (strcmp(event->name, "power_end")==0)
		cpu->go_unidle(time);

#if 0
	unsigned int i;
	for (i = 0; i < system_level.children.size(); i++)
		if (system_level.children[i])
			system_level.children[i]->validate();
#endif
}

void process_cpu_data(void)
{
	unsigned int i;
	system_level.reset_pstate_data();

	perf_events->process();

	for (i = 0; i < system_level.children.size(); i++)
		if (system_level.children[i])
			system_level.children[i]->validate();

}

void end_cpu_data(void)
{
	system_level.reset_pstate_data();

	perf_events->clear();
}

void clear_cpu_data(void)
{
	if (perf_events)
		perf_events->release();
	delete perf_events;
}


void clear_all_cpus(void)
{
	unsigned int i;
	for (i = 0; i < all_cpus.size(); i++) {
		delete all_cpus[i];
	}
	all_cpus.clear();
}
