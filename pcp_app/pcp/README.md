Overview
********

Please consult the assignment sheet for the implementation requirements.

You can build and flash the application via
```bash
$ west build -p always -b esp32_devkitc/esp32/procpu --sysbuild apps/04_pcp
$ west flash
$ west espressif monitor
```

TODO: Part a
************

What do you observe when executing the unmodified pcp_app? Describe how this problem arises (2-3 sentences) by using terminology from the lecture and reference to the Zephyr documentation.

1. We observed "Unbounded Priority Inversion" by T1, T2 and T3. As, T1 was blocked by T3 for R1 and T2 preempted T3, which further delayed T1.
2. We also observed "Deadlock", as T1 has locked R2 but was still waiting for R1 which was held by T3 who is also waiting for R2.

Team Members: Gopalapillai Balagopal, and Kattalaparambi Binesh Neha, and Vaitheeswaran
Kirthika, and Verma, Riya

