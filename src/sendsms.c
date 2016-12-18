#include "config.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "sms-api.h"
#include "web.h"
#include "sendsms.h"
#include "debug.h"

static struct sendsms_cfg config;

#define QUOTE(str) #str
#define EQUOTE(str) QUOTE(str)

int
cfg_export(FILE *fp)
{
	struct sendsms_cfg *cfg = &config;
	if (cfg->username)
		fprintf(fp,"username=%s\n", cfg->username);
	if (cfg->password)
		fprintf(fp,"password=%s\n", cfg->password);
	if (cfg->sourceaddr)
		fprintf(fp,"sourceaddr=%s\n", cfg->sourceaddr);

	return 0;
}


static void
mkdirp(const char *dir)
{
	char tmp[PATH_MAX];
	char *p = NULL;
	size_t len;

	len = snprintf(tmp, sizeof(tmp),"%s",dir);
	if(tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for(p = tmp + 1; *p; p++) {
		if(*p == '/') {
			*p = 0;
			mkdir(tmp, S_IRWXU);
			*p = '/';
		}
	}
	mkdir(tmp, S_IRWXU);
}

int
cfg_save(void)
{
	char path[PATH_MAX];
	char *cp;
	FILE *fp;

	cp = getenv("HOME");
	if (cp) {
		snprintf(path,sizeof(path)-1,"%s/.config/sendsms/",cp);
		mkdirp(path);
		snprintf(path,sizeof(path)-1,"%s/.config/sendsms/sendsmsrc",cp);
		fp = fopen(path,"w");
	} else {
		fprintf(stderr, "Failed to obtain home directory\n");
		return -1;
	}
	cfg_export(fp);
	fclose(fp);
	return 0;
}

int
cfg_load(void)
{
	struct sendsms_cfg *cfg = &config;
	char path[PATH_MAX];
	char *cp, *dp;
	FILE *fp = NULL;
	char line[4096];
	char var[4096];
	char val[4096];

	memset(&config, 0 , sizeof(*cfg));

	/* First check for user config */
	cp = getenv("HOME");
	if (cp) {
		snprintf(path,sizeof(path)-1,"%s/.config/sendsms/sendsmsrc",cp);
		fp = fopen(path,"r");
	}

	if (!fp) {
		snprintf(path,sizeof(path)-1, "%s/sendsmsrc", EQUOTE(SYSCONFDIR));
	}

	if (!fp) {
		return -1;
	}

	while ((cp = fgets(line,sizeof(line),fp)) != NULL) {
		if (*cp == '#')
			continue;
		if (!*(cp + strspn(cp," \t\r\n"))) {
			continue;
		}
		var[0] = '\0';
		val[0] = '\0';
		sscanf(cp,"%[^ =] = %[^\r\n]", var, val);
		// printf("var=[%s] val=[%s]\n", var,val);
		if (!*val) {
			continue;
		}
		if (!strcasecmp(var, "username")) {
			cfg->username = strdup(val);
		} else if (!strcasecmp(var, "password")) {
			cfg->password = strdup(val);
		}
		else if (!strcasecmp(var, "sourceaddr")) {
			dp = val;
			if (*dp == '+')
				dp++;
			cfg->sourceaddr = strdup(dp);
		}
	}

	fclose(fp);
	return 0;
}

static int verbose_flag = 0;

static int
usage(const char *me)
{
	fprintf(stderr,"%s {-f bulkfile|-d recipient} [-u username] [-p password] [-s sourceaddr ]  [-r] message\n"
			"Version: %s\n"
			"-f bulkfile - File with destination number on each line\n"
			"-d recipient - Destination number\n"
			"-u username - Service username\n"
			"-p password - Service password\n"
			"-s sourceaddr - Source number\n"
			"-r            - Request Delivery Report"
			"message - text to send to the destination number (required)\n"
			"\n"
			"You must specify either bulkfile or recipient.\n"
			"Service username is required.\n"
			,me, VERSION
		);
	return -1;
}

#define CFG_EXPORT 1
#define CFG_SAVE 2

int
main(int argc, char **argv)
{
	int rc = - 1;
	int c;
	int action = 0;
	struct sendsms_cfg *cfg = &config;
	char *destaddr = NULL;
	char *destfile = NULL;
	char *message = NULL;
	int sms_mode = CC_AUTO;
	int dlr = 0;

	cfg_load();

	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] =
		{
			/* These options set a flag. */
			{"verbose", no_argument, &verbose_flag, 1},
			{"destaddr", required_argument, 0, 'd'},
			{"destfile",  required_argument, 0, 'f'},
			{"username",  required_argument, 0, 'u'},
			{"password",  required_argument, 0, 'p'},
			{"sourceaddr",  required_argument, 0, 's'},
			{"cfgexport",  no_argument, 0, 'e'},
			{"cfgsave",  no_argument, 0, 'w'},
			{"dlr",  no_argument, 0, 'r'},
			{"gsm7",  no_argument, 0, '7'},
			{"gsm8",  no_argument, 0, '8'},
			{"ucs2",  no_argument, 0, '2'},
			{"help",  no_argument, 0, 'h'},
		    {0, 0, 0, 0}
		};
               c = getopt_long(argc, argv, ":78erwhd:f:u:p:s:",
                        long_options, &option_index);
               if (c == -1)
                   break;

		switch (c) {
			case 'd':
				// printf("destaddr '%s'\n", optarg);
				destaddr = optarg;
				break;
			case 'f':
				// printf("destfile '%s'\n", optarg);
				destfile = optarg;
				break;
			case 'u':
				// printf("username '%s'\n", optarg);
				if (cfg->username)
					free(cfg->username);
				cfg->username = strdup(optarg);
				break;
			case 'p':
				// printf("password '%s'\n", optarg);
				if (cfg->password)
					free(cfg->password);
				cfg->password = strdup(optarg);
				break;
			case 's':
				// printf("source '%s'\n", optarg);
				if (cfg->sourceaddr)
					free(cfg->sourceaddr);
				cfg->sourceaddr = strdup(optarg);
				break;
			case 'e':
				action = CFG_EXPORT;
				break;
			case 'w':
				action = CFG_SAVE;
				break;
			case 'h':
				return usage(argv[0]);
			case '7':
				sms_mode = CC_GSM;
				break;
			case '8':
				sms_mode = CC_BIN;
				break;
			case '2':
				sms_mode = CC_UCS2;
				break;
			case 'r':
				dlr = 1;
				break;
			default:
				fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
                }
	}

	if (action) {
		switch (action) {
			case CFG_SAVE:
				cfg_save();
				break;
			case CFG_EXPORT:
				cfg_export(stdout);
				break;
			default:
				fprintf(stderr, "Unhandled config action\n");
		}
		exit(0);
	}

	debug(1, "u:[%s] p:[%s] s:[%s] d:[%s] f:[%s]\n",
		cfg->username, cfg->password?:"", cfg->sourceaddr, destaddr, destfile);

	if (!destaddr && !destfile && !cfg->username) {
		return usage(argv[0]);
	}

	if (optind < argc) {
		message = argv[optind++];
		if (optind < argc) {
			fprintf(stderr, "extra arguments ignored\n");
			while (optind < argc)
				printf("%s ", argv[optind++]);
			printf("\n");
		}
	} else {
		return usage(argv[0]);
	}

	web_init();
	debug(10, "Message:[%s]\n", message);
	if (destaddr) {
		rc = send_sms(cfg->username, cfg->password, cfg->sourceaddr,
			destaddr, sms_mode, message, dlr);
		debug(1, "res:[%d]\n", rc);
	} else {
		debug(10, "message:[%s] ot file:[%s]\n", message, destfile);
		send_bulk_sms(cfg->username, cfg->password, cfg->sourceaddr, destfile, sms_mode, message, dlr);

	}
	web_cleanup();

	return rc;
}
