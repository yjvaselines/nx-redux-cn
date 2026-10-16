/*
 * ssh_desc.c - Formatter for the Developer page's "启用 SSH / SFTP" hint.
 *
 * Kept free of SDL/api.h so it can be unit-tested on the host
 * (see tests/test_ssh_desc.c).
 */

#include "ssh_desc.h"

#include <stdio.h>

int dev_format_ssh_desc(char* buf, size_t len, int running, int no_password, const char* ip) {
	if (!buf || len == 0)
		return -1;

	int has_ip = (ip && ip[0] != '\0');

	if (!running)
		return snprintf(buf, len, "启动 SSH/SFTP 服务，用于远程访问与文件传输。");

	if (has_ip) {
		if (no_password)
			return snprintf(buf, len, "SSH/SFTP 已启用。root@%s  无密码", ip);
		return snprintf(buf, len, "SSH/SFTP 已启用。root@%s  密码：tina", ip);
	}

	if (no_password)
		return snprintf(buf, len, "SSH/SFTP 已启用。登录：root，无密码（无网络）");
	return snprintf(buf, len, "SSH/SFTP 已启用。登录：root / tina（无网络）");
}
