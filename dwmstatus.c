#define _BSD_SOURCE
#define BUF_SIZE 256
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

// Timezones
char *utcTimeZone = "UTC";
char *madridTimeZone = "Europe/Madrid";

// X11 display
static Display *display;

// Functions
char *printFormattedString(char *format, ...);
void setTimeZone(char *timezone);
char *makeTimes(char *format, char *timezone);
void setStatus(char *status);
char *getLoadAverage(void);
char *readFileContents(char *filepath);
char *getBatteryStatus(char *batteryPath);
char *getTemperature(char *sensorPath);
char *executeScript(char *command);

// Function implementations
char *printFormattedString(char *format, ...) {
    va_list arguments;
    char *result;
    int length;

    va_start(arguments, format);
    length = vsnprintf(NULL, 0, format, arguments);
    va_end(arguments);

    result = malloc(++length);
    if (result == NULL) {
        perror("malloc");
        exit(1);
    }

    va_start(arguments, format);
    vsnprintf(result, length, format, arguments);
    va_end(arguments);

    return result;
}

void setTimeZone(char *timezone) {
    setenv("TZ", timezone, 1);
}

// TIME/DATE FORMATTING
// %W: week number of the year
// %a: abbreviated weekday name according to the current locale (e.g., Sun, Mon, Tue, etc.).
// %d: day of the month as a decimal number (01-31).
// %m: month of the year as a decimal number (01-12).
// %b: abbreviated month name according to the current locale (e.g., Jan, Feb, Mar, etc.).
// %H:%M: Represents the hour (00-23) and minute (00-59) in 24-hour clock format.
// %Z: Represents the timezone abbreviation (e.g., CET, EST, PST).
// %Y: Represents the year as a four-digit number (2023).
// %y: Represents the year as a two-digit number (23).
char *makeTimes(char *format, char *timezone) {
    char buffer[129];
    time_t timeValue;
    struct tm *timeInfo;

    setTimeZone(timezone);
    timeValue = time(NULL);
    timeInfo = localtime(&timeValue);
    if (timeInfo == NULL)
        return printFormattedString("");

    if (!strftime(buffer, sizeof(buffer) - 1, format, timeInfo)) {
        fprintf(stderr, "strftime == 0\n");
        return printFormattedString("");
    }

    return printFormattedString("%s", buffer);
}

void setStatus(char *str) {
    XStoreName(display, DefaultRootWindow(display), str);
    XSync(display, False);
}

char *getLoadAverage(void) {
    double averages[3];

    if (getloadavg(averages, 3) < 0)
        return printFormattedString("");

    return printFormattedString("%.2f %.2f %.2f", averages[0], averages[1], averages[2]);
}

char *readFileContents(char *filepath) {
    char line[513];
    FILE *fileDescriptor;

    memset(line, 0, sizeof(line));

    fileDescriptor = fopen(filepath, "r");
    if (fileDescriptor == NULL)
        return NULL;

    if (fgets(line, sizeof(line) - 1, fileDescriptor) == NULL) {
        fclose(fileDescriptor);
        return NULL;
    }
    fclose(fileDescriptor);

    return printFormattedString("%s", line);
}

char *getBatteryStatus(char *base) {
    char *contents, status;
    int designCapacity, remainingCapacity;

    designCapacity = -1;
    remainingCapacity = -1;

    contents = readFileContents(printFormattedString("%s/%s", base, "present"));
    if (contents == NULL)
        return printFormattedString("");
    if (contents[0] != '1') {
        free(contents);
        return printFormattedString("not present");
    }
    free(contents);

    contents = readFileContents(printFormattedString("%s/%s", base, "charge_full_design"));
    if (contents == NULL) {
        contents = readFileContents(printFormattedString("%s/%s", base, "energy_full_design"));
        if (contents == NULL)
            return printFormattedString("");
    }
    sscanf(contents, "%d", &designCapacity);
    free(contents);

    contents = readFileContents(printFormattedString("%s/%s", base, "charge_now"));
    if (contents == NULL) {
        contents = readFileContents(printFormattedString("%s/%s", base, "energy_now"));
        if (contents == NULL)
            return printFormattedString("");
    }
    sscanf(contents, "%d", &remainingCapacity);
    free(contents);

    contents = readFileContents(printFormattedString("%s/%s", base, "status"));
    if (!strncmp(contents, "Discharging", 11)) {
        status = '-';
    } else if (!strncmp(contents, "Charging", 8)) {
        status = '+';
    } else {
        status = '?';
    }

    if (remainingCapacity < 0 || designCapacity < 0)
        return printFormattedString("invalid");

    return printFormattedString("%.0f%%%c", ((float)remainingCapacity / (float)designCapacity) * 100, status);
}

char *getTemperature(char *sensorPath) {
    char *contents;

    contents = readFileContents(sensorPath);
    if (contents == NULL)
        return printFormattedString("");
    return printFormattedString("%02.2f°C", atof(contents) / 1000);
}

// Custom addition: function to calculate memory usage
char *getMemoryUsage(void) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    char line[BUF_SIZE];
    unsigned long mem_total = 0, mem_free = 0, buffers = 0, sreclaimable = 0, cached = 0;
    // Read each line of /proc/meminfo special file and check if the line matches any of the patterns.
    // If it does, assign the extracted value to the corresponding variable
    while (fgets(line, BUF_SIZE, file)) {
        unsigned long value;
        if (sscanf(line, "MemTotal: %lu kB", &value) == 1) {
            mem_total = value;
        } else if (sscanf(line, "MemFree: %lu kB", &value) == 1) {
            mem_free = value;
        } else if (sscanf(line, "Buffers: %lu kB", &value) == 1) {
            buffers = value;
        } else if (sscanf(line, "SReclaimable: %lu kB", &value) == 1) {
            sreclaimable = value;
        } else if (sscanf(line, "Cached: %lu kB", &value) == 1) {
            cached = value;
        }
    }

    fclose(file);

    // According to i3statusbar and gotop (considers SReclaimable as USED memory)
    // unsigned long buff_cached = cached + sreclaimable + buffers;

    // According to command "free" (considers SReclaimable as FREE memory)
    unsigned long buff_cached = cached + buffers;
    unsigned long kB_used = mem_total - mem_free - buff_cached;
    double MiB_used = (double)kB_used / 1024;

    // For debugging purposes
    // printf("Total Memory: %lu kB\n", mem_total);
    // printf("Cached:       %lu kB\n", cached);
    // printf("Free Memory:  %lu kB\n", mem_free);
    // printf("Buffers:      %lu kB\n", buffers);
    // printf("SReclaimable: %lu kB\n", sreclaimable);
    // printf("buff/cache:   %lu kB\n", buff_cached);
    // printf("Used:         %.1f MiB\n", MiB_used);

    char *result = malloc(20);
    if (result == NULL) {
        return NULL;
    }
    snprintf(result, 20, "%d MiB", (int)MiB_used);
    return result;
}

// Allows capturing, processing, and formatting the output, with better error
// handling within the program, unlike system("cmd");
char *executeScript(char *command) {
    FILE *file;
    char returnValue[1025], *result;

    memset(returnValue, 0, sizeof(returnValue));

    file = popen(command, "r");
    if (file == NULL)
        return printFormattedString("");

    result = fgets(returnValue, sizeof(returnValue), file);
    pclose(file);
    if (result == NULL)
        return printFormattedString("");
    returnValue[strlen(returnValue) - 1] = '\0';

    return printFormattedString("%s", returnValue);
}

int main(void) {
    char *status, *loadAverages, *battery, *timeMadrid, *temperature0, *temperature1, *keyboardMap, *memoryUsage;

    if (!(display = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    for (;; sleep(1)) {
        loadAverages = getLoadAverage();
        battery = getBatteryStatus("/sys/class/power_supply/BAT0");

        memoryUsage = getMemoryUsage();
        timeMadrid = makeTimes(" %d/%m/%y  %H:%M:%S ", madridTimeZone);
        keyboardMap = executeScript("setxkbmap -query | grep layout | cut -d':' -f 2- | tr -d ' '");
        temperature0 = getTemperature("/sys/class/hwmon/hwmon2/temp1_input");
        temperature1 = getTemperature("/sys/class/hwmon/hwmon1/temp1_input");

        status = printFormattedString(" Mem %s | KB:%s | %s %s | L:%s | %s",
            memoryUsage, keyboardMap, temperature0, temperature1, loadAverages, timeMadrid);
        setStatus(status);

        free(memoryUsage);
        free(keyboardMap);
        free(temperature0);
        free(temperature1);
        free(loadAverages);
        free(battery);
        free(timeMadrid);
        free(status);
    }

    XCloseDisplay(display);

    return 0;
}
