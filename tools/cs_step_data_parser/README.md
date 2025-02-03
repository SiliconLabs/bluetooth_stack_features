# Logging and parsing raw BLE Channel Sounding step data

This document describes the steps required in order to log and parse raw Channel Sounding (CS) step data, using Silicon Labs CS sample applications. For more information on our CS solution,
please refer to our [official CS documentation.](https://docs.silabs.com/rtl-lib/latest/rtl-lib-channel-sounding-getting-started/)

> [!NOTE]  
> This method has been developed and tested using SiSDK-2024.12.1.

## Collecting the logs

The step data is collected as logs from the CS initiator at the end of each CS procedure. This includes the step data both from the initiator and the reflector. To enable the logging on the initiator, please follow the steps below.

### Modifying the cs_initiator source code

1. **Create a project based on the *bt_cs_soc_initiator* sample application**

2. **For improved logging stability, make sure SL_IOSTREAM_EUSART_VCOM_BAUDRATE in *config/sl_iostream_eusart_vcom_config.h* is set to 921600.
     We are logging a lot of data, so this setting is critical.**

3. **Go to *simplicity_sdk_\<version\>/app/bluetooth/common/cs_initiator/src/cs_initiator.c***
    * Add the following to the file:

    **Definition**

    ```C
    // -----------------------------------------------------------------------------
    // Print Step Data
    static void print_step_data(cs_initiator_t *initiator)
    {
    sl_rtl_cs_subevent_data *initiator_subevent_data = initiator->cs_procedure.initiator_subevent_data;
    sl_rtl_cs_subevent_data *reflector_subevent_data = initiator->cs_procedure.reflector_subevent_data;
    uint8_t subevent_data_count = initiator->cs_procedure.initiator_subevent_data_count;
    uint16_t init_data_len = initiator_subevent_data->step_data_count;
    uint16_t ref_data_len =  reflector_subevent_data->step_data_count;
    static bool stack_version_read = false;
    static uint16_t major_v = 0;
    static uint16_t minor_v = 0;
    static uint16_t patch_v = 0;

    // Read stack version once
    if (stack_version_read == false) {
        uint16_t build;
        uint32_t bootloader;;
        uint32_t hash;
        
        sl_status_t sc = sl_bt_system_get_version(&major_v, &minor_v, &patch_v, &build, &bootloader, &hash);
        if (sc == SL_STATUS_OK) {
            stack_version_read = true;
        }
        else {
            printf("Failed to read stack version with reason: %d", sc);
        }
    }

    // Print the CS step data, starting with the initiator
    printf("------------Start CS Step Data------------\n");
    if (stack_version_read == true) {
        printf("BLE Stack Version: %d.%d.%d\n", major_v, minor_v, patch_v);
    }
    printf("Subevent data count: %d\n", subevent_data_count);
    printf("Initiator:\n subevent_timestamps_ms: %" PRIu64 "\n Frequency compensation: %d\n Reference power level: %d\n Number of antenna paths: %d\n Step data count: %d\n Step data: ",
            initiator_subevent_data->subevent_timestamp_ms,
            initiator_subevent_data->frequency_compensation,
            initiator_subevent_data->reference_power_level,
            initiator->cs_procedure.cs_config.num_antenna_paths,
            init_data_len);

    for (uint16_t i = 0; i < init_data_len; i++) {
        printf("%02x", initiator_subevent_data->step_data[i]);
    }
    printf("\n");

    // Continue printing the data from the reflector
    printf("Reflector:\n subevent_timestamps_ms: %" PRIu64 "\n Frequency compensation: %d\n Reference power level: %d\n Number of antenna paths: %d\n Step data count: %d\n Step data: ",
            reflector_subevent_data->subevent_timestamp_ms,
            reflector_subevent_data->frequency_compensation,
            reflector_subevent_data->reference_power_level,
            initiator->cs_procedure.cs_config.num_antenna_paths,
            ref_data_len);

    for (uint16_t i = 0; i < ref_data_len; i++) {
        printf("%02x", reflector_subevent_data->step_data[i]);
    }
    printf("\n");
    printf("-------------End CS Step Data-------------\n");
    }
    ```

    **Declaration**

    ```C
    static void print_step_data(cs_initiator_t *initiator);
    ```

    **Includes**
    
    ```C
    #include "printf.h"  // tinyprintf
    ```

4. **Go to the *state_wait_reflector_procedure_complete_on_cs_result()* function**
    * Under the CS_PROCEDURE_STATE_COMPLETED case, comment out *calculate_distance(initiator->conn_handle);*

        * The calculation is not needed if we only are interested in the raw step data

    * Add *print_step_data(initiator);*

    ```C
    case CS_PROCEDURE_STATE_COMPLETED:
      if (initiator->initiator_data.procedure_counter == initiator->reflector_data.procedure_counter) {
        // calculate_distance(initiator->conn_handle);
        sc = app_timer_stop(&initiator->timer_handle);
        print_step_data(initiator);
        if (sc == SL_STATUS_OK) {
            .
            .
            .
    ```

<br/>

After following these steps, the raw step data should be printed to the serial output after each CS procedure. To read the output, open a serial connection using your favourite terminal application. After one CS procedure, the output should look like this:

```text
------------Start CS Step Data------------
BLE Stack Version: 9.0.0
Subevent data count: 3
Initiator:
 subevent_timestamps_ms: 2934
 Frequency compensation: 746
 Reference power level: -33
 Number of antenna paths: 4
 Step data count: 1752
 Step data: 000b0500d901ea0200460500d802e80200210500d501e802022d151394b1ec03ebdf39036126d5033cc21803000000130243150927b13d03fc830503ae81e6038604e20301f0ff130221150838102b03730e280326c2f0038227190303100013020415123b5dfb03815f2f0356af7b03d86dca03d08dca23022815074c611703102e1d03bfdcfa0324726c03003000130222150b2d91e1030f203003aeb12003fd56d303f496d223024b15026821bb0342a0b80399ffd903fed30a0304e40923021c15018f31d60396b128038517cf03a5102b03ad002b23023a15078b02030387413003795d36039c551d039e551c2302201515353aac038972ea0387fdfb034e81d90300f0ff1302311512ee3fc203268e1303ffa92c037f3de9037e6de923022b1515ba5a4a03000fcb03006f1b0374ddf103fdffff13021215069b730703461ecd0375b3fc03b3fe8503973e8623021415068c1c0d0396122803d08c140308b372031b737223022a151562854803f55c1703db210a0377ff290374ff292302441514095cfb0349104d03eaa021034e2e3c0301c0ff13020d15002620c2033bbd230330893f03f46ece03ffefff13023f1513dd9edd0319b4030319ffad03c871cb0300e0ff1302131517eff2dd036af7dd03bad0350316403903001000130239150a0f6ec503c30e2403b0dc0003d60b4603fe1f0013024515076cbfdc0328d2c503ddb30c036740b3036240b32302161500e631d2036c8cfe031f39c20399f1d2039c11d3230209150b940f3503295df0037f2dcf03afee7c03fe4f00130249151634d049039f1023030a7cf903b92e4403ff2f0013021d1517d8ee2a030d493f03339fd6031bcedb03ffffff130234150d8553e4031b70cf0339cbbc03e89deb03e6bdeb23024215164d804e03e39022030dccf7030b5e3603037e3623020a1517dc812d0340e4690324dd10031ffcf5031c1cf7230248150741b2fd03c2632503af4e3b0391640f0395840f2302081514e0cf2e03c657f1033ae3f803a01d3203001000130203150c505c180351ff290371e71d03c3821003c1021123020215103c193903db9ed803ef41c803412d0e03ff2f0013023b1514c2113d03ffd3bf033f81dd036b8301036853002302261507ae701e03697d0a035c9de503f40e7103088f712302271504f018f1031f5e0a033fcfd603ad41d503aa11d52302151504e22c8e03455dda035c93f0030e43ec0301f0ff13020b1513630c0d039d70ca0358180a03dd41c60394a2c3230225151741b1e503a7e6d403e4df3003c8a11f03c1e11f23021b150387d8ce03d530d003e18c0a036a51da036d61da23021e150aa1aeda03b63f2c0386fde703a9fa5b03a07a5b230211150f526e7903ee0cde03d0aceb0305503a03fb4f3a2302321509e3ae3a0309a222033c22f6036f66f3037256f323021f150a99820d03634ede03c502f6032410830339608323020f150722b3e2038a2f3c039ac034034977d60303f0ff13023c150de05c2b03f1d03603cde42d0373d20a0372e20a23024615047664e403bc21e90356540403dd903c0301100013023d1512897ec2037f2e2003a4bb3303639cf90365dcf823024015037131b2031683db03f8afd9036e1321036c432123022f150339454403b46f2d03d7810f03a7ac1703fe1f00130224150e6570d503a439c80363f2e203de3d0103df1d0123020715082eccee0358ad0f0382512d03b6036d03012000130237151431013d03ba74c0038c81df033643070301000013024c151089813f0317ac0e03689047033e0121030000001302411500dae30a03b7d1e50376b4dd03eda03d03f9703d23024a150a2b6dd303171f22035c3c2a035f0d3a0358bd39230205150ef6f304038d9aa80321e2e403eb4ddb03f49dda23020e15076f9cf503ee82d803bed1d303bf08d503bb38d423021a1500379de503d7bf34031beb6503f5cde103f6bde12302231517dd7df60323aba803c8b2f1035a91da035551da230206150d2ca2e403f3e301038eeaa703edddd903feefff13022e1506d2c12203e761f403d1ae3503bb26f503c1c6f4230238150c88503403b69c24030ac5350365e20e0364520f230229150958d03203598216032931e603fe45c70311b6c8230235150b7981200322fc0103a25e2d0392225b038f925b230230150b081e0e03be40c603d03de1038f991a03fc0f00130210150c4f2c0c03012d170381046303a7f2270301000013023e1513f8af2703a53cdc03617e5003ccac1e03cdbc1e23020c1502db42d5034cc8e8037e5cf90343d1d20344e1d223022c1515ade53d03099d1a03ea310903cb6f2c03fe1f001302471517d80df6031b2cd603a222d303a5fdc303ffdfff130233150a60af3b0335e2f0035e121f033c86ea0302e0ff13023615013e7ee203ff40cf03d4dcaa03dbf3f60301f0ff13 
Reflector:
 subevent_timestamps_ms: 2944
 Frequency compensation: 0
 Reference power level: -34
 Number of antenna paths: 4
 Step data count: 1746
 Step data: 000b0300d60100460300d80200210300d201022d1513478ef103ff02d903e659e2030dd0db0312e0db2302431509c4a0bd03562ddb03ae0dfc039edb0e039c2b0f230221150831a0dc03e7d1d703c0ad0403f648f203f2c8f1230204151299afd203403d0403f7d81e031442d60301e0ff13022815073baf1b03d85df80319ced003519d5f035efd5f230222150b13eeef030d03ef0352c1e303a8bab803a51ab923024b1502340ec60355fce40399dee0031ea3d20301f0ff13021c1501969de603fc61e8035bdba103c5c2ee0300e0ff13023a1507d14d150335edf303918ebc037e3c42037f5c422302201515ba36d503a6cf3003de81e803f2c02003f4c0202302311512089d2d03d4c11203fa4514033b902603ff1f0013022b151581a448034cec09032e811703d33e1e03d3ae1d23021215064e203003cb12e1031b2138034026c60301000013021415061870d103f1fc1703758fc60311492203fd1f0013022a1515c69a3d03edefc5035ebe0f034cdee8034bdee823024415140c0c1503d8a23603cbe0220342ef380301b00013020d1500817ddf03c1ff3403ea4f7603b3bcf003b95cef23023f1513939fd90351f4fe03bbadbc03ba21d40300e0ff13021315173f5ed1030b7d97038ca3f103f632fd03f6f2fc230239150a61cc2703e5511b0309302d03207521030200001302451507d24ee003ff5fc403ac43e203af8cd103b21cd12302161500734e260357430703ed66260382be3103fe1f00130209150b2f4d1503feced003ce10cd03ce294103bf494023024915163a532c03de102203445c1c0362103f0362103f23021d15173af21a03490460032d3d0b0300ee1503ff1f00130234150dbcc13c03855208031913b103e781e803e881e8230242151691323b03ac002403e2eb0b03d19e3303d4de3323020a1517ea3cf503a7e8f5032311d0038052de0302f0ff13024815072ede1503398c11037c0dcc03f32d3c03f3ed3b23020815141053fe03da5d8f039b2fd003c3721c03000000130203150cda921b03a442f103e8ac9703658fd503ffefff1302021510289e6f0357adf103faaecc03cdce2503c06e2523023b1514d852ca03c8daf203ca6dec033eafd103fd6fff130226150734e01e03cd6d05032c5de3033160690319c06923022715049516fd03e911fe036ce022032f9e2d032f7e2d2302151504ec56dd030293e60319002e039a10370300100013020b1513c3a1330393bd0803c2d36d03a62df103afddf42302251517426ef00399fac10311e3ec0307c1e00306a1e023021b150343f71d033f7f270305c3fc039d8e29039fae2923021e150a1cddfb039b912103bf2d0d037cc26c03001000130211150fdaca5103dffed303e69dd103d02d2c03cb8d2b2302321509dde1c503e05edc03c6adf7031d9a0d03180a0d23021f150aa01127037a50d8031e4210034113990301c0ff13020f1507e49ecc030fc30a0388c3fc03a23d9303ad9d9223023c150d51cdc803f01c03032a6e4c035bae1e03ff0f0013024615041dac1f03bfdd03036fbce803ca3fbf03ffefff13023d1512678d370349921003f694060328a12e030000001302401503247cd6032390ca0382eee103a663de0301000013022f1503703b4303f25dec032f9e0e0382bfc103ffdfff130224150ebb80210383a6e603f2ce3003e001f30300f0ff1302071508fd52e9034b71d5033e6dee03e368e603fcffff1302371514f332cf03715aef03d16deb037dafd403ffefff13024c15107da23003001c1103de8f400355c0260301100013024115003beddf03a01d0203c62b1a035fa0bd0300e0ff13024a150a6f7e3c031852110300f32a031ac4fd0302f0ff130205150ec6bc0c03a5562d03695e25035d92190300100013020e150797d22203fa5c1003323d2003c7554503cce54423021a1500583dfb03c9603003c26f7703191df503feffff130223151749d1e40308f5b703738031039ec118039af118230206150d0ede2103baec070351d638033e721d0347021d23022e150687dfda03181ef6037f22d203c899f903fc0f00130238150c363df80342bec103176d4b03044e1903fe0f0013022915096013f60327a1e0039a9eea031c5bbd03fdefff130235150be76e2303e38cd4036a7d0a03701f5b03771f5b230230150bf9510b03914d3103a78025032046020303f0ff130210150cb1a1d5030211ca03d6b8fd0392bcff03904c0023023e15139a511e03459c200321a42803c26f330300200013020c1502bfdc0103e2245703f2512a030f9d1603fe1f0013022c1515cc1a390322d0c5033b6e0d033beee703feefff130247151756c1e203f4a0bc033683260369c3e00301e0ff130233150a9cb2ce03d91df0034e5fd803266afb03fd0f0013023615014dc1dd03b1b2f80317e1a603e8722f0301000013
-------------End CS Step Data-------------
```

## Parsing the step data

The step data seen in the log contains the mode-specific data for each step in the procedure. Every step starts with a 3-byte header, that has the following format:

```text
Byte 1 = Step_Mode
Byte 2 = Step_Channel
Byte 3 = Step_Data_Length
```

After the header, the next "Step_Data_Length" bytes is the actual step data.
As an example, we will use a chunk of the log from the initiators step data, seen above:

```text
000b0500d901ea0200460500d802e80200210500d501e802022d151394b1ec03ebdf39036126d5033cc2180300000013
``` 

If we start from the first byte, we can see that:

```text
Step_Mode = 0x00
Step_Channel = 0x0b
Step_Data_Length = 0x05
```

If we move 5 bytes forward in the array, we get to the next header and use that to parse the next step. When doing this for the whole chunk, we get the following structure for the step data:

```text
00 0b 05 00 d9 01 ea 02
00 46 05 00 d8 02 e8 02
00 21 05 00 d5 01 e8 02
02 2d 15 13 94 b1 ec 03 eb df 39 03 61 26 d5 03 3c c2 18 03 00 00 00 13
```

As can be seen, the chunk has 3 mode-0 steps with data length 5, and 1 mode-2 steps with data length 0x15 = 21.

### Parser script

The [parser script](parser/step_data_parser.py) is intended to work as an example on how to parse the raw step data into JSON-format. The script parses the step data based on *BLE Core Specification Version 6.0 | Vol 4, Part E Section 7.7.65.44*. 

To use the script, start by installing the required Python packages from *requirements.txt*:

```bash
pip install -r requirements.txt
```

Once the installation is finished, the script can be started:

```bash
python step_data_parser.py <input.txt> <output.json> 
```

> [!TIP]  
> To test the script, use the included example logs as inputs (for example [example_log_rtt.txt](parser/example_log_rtt.txt)). The output should look like the included example outputs (for example [example_output_rtt.txt](parser/example_output_rtt.json)).
