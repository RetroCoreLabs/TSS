# Extract DKADR logical->physical pairs from an nd100x --trace.
# DKADR entry is PC 010006 (T = logical sector); the driver then issues
# IOX 503 (A = physical CDC block address) to load the controller.
/^010006 / {
    for (i = 1; i <= NF; i++)
        if ($i ~ /^T=/) { logical = substr($i, 3); have = 1 }
}
/IOX 503/ && have {
    for (i = 1; i <= NF; i++)
        if ($i ~ /^A=/) { print logical " -> " substr($i, 3); have = 0; break }
}
