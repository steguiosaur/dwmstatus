#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>

char *tzmanila = "Asia/Manila";
static Display *dpy;

char *smprintf(char *fmt, ...) {
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if (ret == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

void settz(char *tzname) {
    setenv("TZ", tzname, 1);
}

char *mktimes(char *fmt, char *tzname) {
    char buf[129];
    time_t tim;
    struct tm *timtm;

    settz(tzname);
    tim = time(NULL);
    timtm = localtime(&tim);
    if (timtm == NULL)
        return smprintf("");

    if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
        fprintf(stderr, "strftime == 0\n");
        return smprintf("");
    }

    return smprintf("%s", buf);
}

void setstatus(char *str) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}

char *loadavg(void) {
    double avgs[3];

    if (getloadavg(avgs, 3) < 0)
        return smprintf("");

    return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *readfile(char *base, char *file) {
    char *path, line[513];
    FILE *fd;

    memset(line, 0, sizeof(line));

    path = smprintf("%s/%s", base, file);
    fd = fopen(path, "r");
    free(path);
    if (fd == NULL)
        return NULL;

    if (fgets(line, sizeof(line)-1, fd) == NULL) {
        fclose(fd);
        return NULL;
    }
    fclose(fd);

    return smprintf("%s", line);
}

char *getwifi(void) {
    FILE *fp;
    char path[1035];
    char ssid[1035] = "";
    int wifi_on = 0;

    // Check if Wi-Fi is enabled
    fp = popen("nmcli radio wifi", "r");
    if (fp == NULL) {
        return smprintf("Wi-Fi: error");
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        if (strncmp(path, "enabled", 7) == 0) {
            wifi_on = 1;
        }
    }
    pclose(fp);

    if (wifi_on) {
        // Get the SSID of the connected network
        fp = popen("nmcli -t -f active,ssid dev wifi | grep '^yes' | cut -d: -f2", "r");
        if (fp == NULL) {
            return smprintf("Wi-Fi: error");
        }

        if (fgets(ssid, sizeof(ssid) - 1, fp) != NULL) {
            // Remove newline character from the end of the SSID
            ssid[strcspn(ssid, "\n")] = '\0';
            pclose(fp);

            // Truncate SSID if it is longer than 5 characters
            if (strlen(ssid) > 5) {
                char truncated_ssid[9]; // 5 characters + "..." + null terminator
                snprintf(truncated_ssid, sizeof(truncated_ssid), "%.2s...", ssid);
                return smprintf("Wi-Fi: %s", truncated_ssid);
            } else {
                return smprintf("Wi-Fi: %s", ssid);
            }
        }

        pclose(fp);
        return smprintf("Wi-Fi: on");
    } else {
        return smprintf("Wi-Fi: off");
    }
}

char *getbattery(char *base) {
    char *co, status;
    int capacity = -1;

    co = readfile(base, "present");
    if (co == NULL)
        return smprintf("");
    if (co[0] != '1') {
        free(co);
        return smprintf("not present");
    }
    free(co);

    co = readfile(base, "capacity");
    if (co == NULL) {
        return smprintf("invalid");
    }
    sscanf(co, "%d", &capacity);
    free(co);

    co = readfile(base, "status");
    if (co == NULL) {
        status = '?';
    } else if (!strncmp(co, "Not charging", 12)) {
        status = '*';
    } else if (!strncmp(co, "Discharging", 11)) {
        status = '-';
    } else if (!strncmp(co, "Charging", 8)) {
        status = '+';
    } else if (!strncmp(co, "Full", 4)) {
        status = '^';
    } else {
        status = '?';
    }
    free(co);

    if (capacity < 0)
        return smprintf("invalid");

    return smprintf("%d%%%c", capacity, status);
}

char *gettemperature(char *base, char *sensor) {
    char *co;

    co = readfile(base, sensor);
    if (co == NULL)
        return smprintf("");
    return smprintf("%02.0f°C", atof(co) / 1000);
}

char *getvolume(void) {
    FILE *fp;
    char path[1035];
    int volume_level = -1;

    fp = popen("pamixer --get-volume", "r");
    if (fp == NULL) {
        return smprintf("err");
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        sscanf(path, "%d", &volume_level);
    }
    pclose(fp);

    if (volume_level >= 0) {
        return smprintf("%d%%", volume_level);
    } else {
        return smprintf("Unknown");
    }
}

char *getbrightness(void) {
    FILE *fp;
    char path[1035];
    int brightness_level = -1;
    int max_brightness = 852; // Update with the maximum brightness value of your system

    fp = fopen("/sys/class/backlight/intel_backlight/brightness", "r");
    if (fp == NULL) {
        return smprintf("err");
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        sscanf(path, "%d", &brightness_level);
    }
    fclose(fp);

    if (brightness_level >= 0) {
        int percent_brightness = (brightness_level * 100 + max_brightness / 2) / max_brightness; // Round to nearest integer
        return smprintf("%d%%", percent_brightness);
    } else {
        return smprintf("Unknown");
    }
}

char *getram(void) {
    FILE *fp;
    char line[128];
    long total = 0, available = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return smprintf("err");
    }

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "MemTotal: %ld kB", &total);
        sscanf(line, "MemAvailable: %ld kB", &available);
    }
    fclose(fp);

    long used = total - available;
    return smprintf("%.1f%%", (double)used / total * 100);
}

int main(void) {
    char *status;
    char *avgs;
    char *bat0;
    char *bat1;
    char *tmmnl;
    char *t0;
    char *t1;
    char *wifi;
    char *vol;
    char *bright;
    char *ram;

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    for (;; sleep(1)) {
        avgs = loadavg();
        bat0 = getbattery("/sys/class/power_supply/BAT0");
        bat1 = getbattery("/sys/class/power_supply/BAT1");
        tmmnl = mktimes("%a %y-%m-%d %H:%M", tzmanila);
        t0 = gettemperature("/sys/devices/virtual/thermal/thermal_zone0", "temp");
        t1 = gettemperature("/sys/devices/virtual/thermal/thermal_zone1", "temp");
        wifi = getwifi();
        vol = getvolume();
        bright = getbrightness();
        ram = getram();

        status = smprintf(" [%s|%s] [%s] [%s] [%s %s B:%s V:%s] [%s] [%s] ",
                t0, t1, avgs, ram, bat0, bat1, bright, vol, tmmnl, wifi);
        setstatus(status);

        free(t0);
        free(t1);
        free(avgs);
        free(bat0);
        free(bat1);
        free(tmmnl);
        free(wifi);
        free(bright);
        free(vol);
        free(status);
        free(ram);
    }

    XCloseDisplay(dpy);

    return 0;
}

