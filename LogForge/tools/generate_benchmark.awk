BEGIN {
    if (records == "") {
        records = 1000000
    }

    levels[0] = "INFO"
    levels[1] = "WARN"
    levels[2] = "ERROR"
    services[0] = "auth"
    services[1] = "inventory"
    services[2] = "payments"

    for (record_number = 0; record_number < records; ++record_number) {
        printf "2026-08-15T10:15:03 %s %s Deterministic benchmark record %08d\n", \
            levels[record_number % 3], services[(record_number * 2) % 3], record_number
    }
}
