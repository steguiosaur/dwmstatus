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
    char *icon = "";
    char strength_str[5] = "";

    // Check if Wi-Fi is enabled
    fp = popen("nmcli radio wifi", "r");
    if (fp == NULL) {
        return smprintf("󰂎 error");
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        if (strncmp(path, "enabled", 7) == 0) {
            wifi_on = 1;
        }
    }
    pclose(fp);

    if (wifi_on) {
        // Get Wi-Fi status and SSID and strength
        fp = popen("nmcli -t -f active,ssid,signal dev wifi | grep '^yes'", "r");
        if (fp == NULL) {
            return smprintf("󱛏 ERR");
        }

        if (fgets(path, sizeof(path) - 1, fp) != NULL) {
            sscanf(path, "yes:%[^:]:%s", ssid, strength_str);
            // Remove newline character from the end of SSID and strength if present (though -t should prevent it for ssid and signal)
            ssid[strcspn(ssid, "\n")] = '\0';
            strength_str[strcspn(strength_str, "\n")] = '\0';
            pclose(fp);

            int strength = -1;
            if (sscanf(strength_str, "%d", &strength) == 1) {
                if (strength >= 75) {
                    icon = "󰤨"; // Wifi Strength 4 (Excellent)
                } else if (strength >= 50) {
                    icon = "󰤥"; // Wifi Strength 3 (Good)
                } else if (strength >= 25) {
                    icon = "󰤢"; // Wifi Strength 2 (Fair)
                } else if (strength >= 1) {
                    icon = "󰤟"; // Wifi Strength 1 (Weak)
                } else {
                    icon = "󰤯"; // Wifi Strength 0 (Very Weak/None)
                    return smprintf("%s %s", icon, "0%");
                }
                // Truncate SSID if it is longer than 5 characters
                if (strlen(ssid) > 5) {
                    char truncated_ssid[9]; // 5 characters + "..." + null terminator
                    snprintf(truncated_ssid, sizeof(truncated_ssid), "%.4s…", ssid);
                    return smprintf("%s %s", icon, truncated_ssid); // Icon and truncated SSID
                } else {
                    return smprintf("%s %s", icon, ssid); // Icon and SSID
                }
            } else {
                return smprintf("󱛏 ERR"); // Could not parse strength, unknown error.
            }

        } else { // Wi-Fi is on but not connected to a network (no SSID)
            pclose(fp);
            icon = "󰤫"; // On (but not connected) icon
            return smprintf("%s on", icon);
        }

    } else {
        icon = "󰤭"; // Off icon
        return smprintf("%s off", icon);
    }
}

char *getbattery(char *base) {
    char *co, status;
    int capacity = -1;
    char *icon = ""; // Default icon (empty if no battery)

    co = readfile(base, "present");
    if (co == NULL)
        return smprintf("");
    if (co[0] != '1') {
        free(co);
        return smprintf("󱉞 NP"); // Unknown icon for not present
    }
    free(co);

    co = readfile(base, "capacity");
    if (co == NULL) {
        return smprintf("󰂃 ERR"); // Unknown icon for invalid
    }
    sscanf(co, "%d", &capacity);
    free(co);

    co = readfile(base, "status");
    if (co == NULL) {
        status = '?';
        icon = "󰂃"; // Unknown icon for status error
    } else if (!strncmp(co, "Not charging", 12)) {
        status = '*';
        if (capacity <= 10) icon = "󰁺";
        else if (capacity <= 20) icon = "󰁻";
        else if (capacity <= 30) icon = "󰁼";
        else if (capacity <= 40) icon = "󰁽";
        else if (capacity <= 50) icon = "󰁾";
        else if (capacity <= 60) icon = "󰁿";
        else if (capacity <= 70) icon = "󰂀";
        else if (capacity <= 80) icon = "󰂁";
        else if (capacity <= 90) icon = "󰂂";
        else icon = "󰁹";
    } else if (!strncmp(co, "Discharging", 11)) {
        status = '-';
        if (capacity <= 10) icon = "󰁺";
        else if (capacity <= 20) icon = "󰁻";
        else if (capacity <= 30) icon = "󰁼";
        else if (capacity <= 40) icon = "󰁽";
        else if (capacity <= 50) icon = "󰁾";
        else if (capacity <= 60) icon = "󰁿";
        else if (capacity <= 70) icon = "󰂀";
        else if (capacity <= 80) icon = "󰂁";
        else if (capacity <= 90) icon = "󰂂";
        else icon = "󰁹";
    } else if (!strncmp(co, "Charging", 8)) {
        status = '+';
        if (capacity <= 10) icon = "󰢜";
        else if (capacity <= 20) icon = "󰂆";
        else if (capacity <= 30) icon = "󰂇";
        else if (capacity <= 40) icon = "󰂈";
        else if (capacity <= 50) icon = "󰢝";
        else if (capacity <= 60) icon = "󰂉";
        else if (capacity <= 70) icon = "󰢞";
        else if (capacity <= 80) icon = "󰂊";
        else if (capacity <= 90) icon = "󰂋";
        else icon = "󰂅";
    } else if (!strncmp(co, "Full", 4)) {
        status = '^';
        icon = "󱟢"; // Full
    } else {
        status = '?';
        icon = "󰂃"; // Unknown icon for unknown status
    }
    free(co);

    if (capacity < 0)
        return smprintf("󰂃 ERR"); // Unknown icon for invalid capacity

    return smprintf("%s %d%%%c", icon, capacity, status); // Return icon and text
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
    char *icon = "";

    fp = popen("pamixer --get-volume", "r");
    if (fp == NULL) {
        return smprintf("󱄡 ERR"); // Volume Error icon
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        sscanf(path, "%d", &volume_level);
    }
    pclose(fp);

    if (volume_level >= 0) {
        if (volume_level == 0) icon = "󰝟"; // Mute
        else if (volume_level <= 30) icon = "󰕿"; // Low
        else if (volume_level <= 70) icon = "󰖀"; // Medium
        else icon = "󰕾"; // High
        return smprintf("%s %d%%", icon, volume_level); // Return icon and text
    } else {
        return smprintf("󰸈 E%"); // Volume Error icon
    }
}


char *getbrightness(void) {
    FILE *fp;
    char path[1035];
    int brightness_level = -1;
    int max_brightness = 852; // Update with the maximum brightness value of your system
    char *icon = "";

    fp = fopen("/sys/class/backlight/intel_backlight/brightness", "r");
    if (fp == NULL) {
        return smprintf("󰂎 err"); // Unknown icon for error
    }

    if (fgets(path, sizeof(path) - 1, fp) != NULL) {
        sscanf(path, "%d", &brightness_level);
    }
    fclose(fp);

    if (brightness_level >= 0) {
        int percent_brightness = (brightness_level * 100 + max_brightness / 2) / max_brightness;
        if (percent_brightness <= 15) icon = "󰃞"; // Low Brightness
        else if (percent_brightness <= 40) icon = "󰃟"; // Chill Brightness
        else if (percent_brightness <= 70) icon = "󰃝"; // Medium Brightness
        else icon = "󰃠"; // High Brightness
        return smprintf("%s %d%%", icon, percent_brightness);
    } else {
        return smprintf("󰳲 E%"); // Unknown icon for unknown brightness
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

        status = smprintf("   %s:%s | 󰍛 %s | 󰓅 %s | %s %s | %s | %s | 󰃭 %s | %s ",
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
