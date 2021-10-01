""" Settings file """
############
# SETTINGS #
############

import sys

run = dict(
   LOCATION_ID = "JSI",
   PROCESS     = True,
)

try:
   locfile = open('/boot/SAAM/location.id', 'r')
   line = locfile.readline().strip()
   locfile.close()
   if line:
      run['LOCATION_ID'] = line
except:
   pass
   # sys.exit(1)

###########################################
# Logging parameters define:
# LEVEL: logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
# FILE: 
#   path         -  log file
#   empty        -  system log
#   not defined  -  console
###########################################
log_parameters = dict(
   FILE = "{}.log",
   # FILE = "",
   SIZE = 8,  # Log size 8MB
   CNT  = 2,  # Log rotate count = 2
   FORMAT = "%(asctime)s %(levelname)s: %(message)s",
   DATE = "%b %d %H:%M:%S",
   LEVEL = "info"
   # LEVEL = "debug"
)

#####################################################
# Sensors define which sensors are being collected
# Location specifics
# Added: 20. Mar. 2019
# Precision defines numeric precision of measurment
# data sent to the server. Local processing is still
# performed on non-rounded values
#####################################################
sensors = dict(
    HUMIDITY = dict(
        type = "MALOS",
        config = dict(
            driver = "humidity",  # Name of the MALOS driver component
            sensor = "Humidity",  # Name of the MALOS sensor component
            desc = "Temperature and Humidity sensor",
            base = 20017,
            sample_time = 2.0,
            keep_alive = 6.0,
            calibrate = dict(
                current_temperature = 23,
            )
        ),
        quantity = dict(
            # data_id      sensor_ids
            humidity    = 'humidity',
            temperature = 'temperature',
            # with filters:
            # data_id         channel         filter        N
            # humidity_a  = ('humidity',    'avg_sample',  10),
            # temp_avg    = ('temperature', 'avg_sample',  30),
        ),
    ),
    PRESSURE = dict(
        type = "MALOS",
        config = dict(
            driver = "Pressure",  # Name of the MALOS driver component
            sensor = "Pressure",  # Name of the MALOS sensor component
            desc = "Pressure, Altitude, and Temperature sensor",
            base = 20025,
            sample_time = 2.0,
            keep_alive = 6.0,
        ),
        quantity = dict(   
            # data_id      sensor_ids
            pressure    = 'pressure',
            altitude    = 'altitude',
            temp_raw   = 'temperature',
        ),
    ),
    MOTION = dict(
        type = "MALOS",
        config = dict(
            driver = "Imu",  # Name of the MALOS driver component
            sensor = "Imu",  # Name of the MALOS sensor component
            desc = "IMU sensor",
            base = 20013,
            sample_time = 0.02,
            # sample_time = 1,
            keep_alive = 6.0,
        ),
        quantity = dict(   
            # data_id      sensor_ids
            # quantity           channels with description                    filter      N
            angle       = ({ 'x': 'roll',    'y': 'pitch',   'z': 'yaw' }, 'avg_sample', 600),
            accel       = { 'x': 'accel_x', 'y': 'accel_y', 'z': 'accel_z' },
            # gyro        = { 'x': 'gyro_x',  'y': 'gyro_y',  'z': 'gyro_z' },
            # mag         = { 'x': 'mag_x',   'y': 'mag_y',   'z': 'mag_z' },
        ),
    ),
    UV = dict(
        type = "MALOS",
        config = dict(
            driver = "UV",  # Name of the MALOS driver component
            sensor = "UV",  # Name of the MALOS sensor component
            desc = "UV sensor",
            base = 20029,
            sample_time = 1.0,
            keep_alive = 6.0,
        ),
        quantity = dict(   
            # data_id      sensor_ids
            uv_index    = 'uv_index',
            uv_risk     = 'oms_risk',
        ),
    ),
    AUDIO = dict(
        type = "Audio",
        config = dict(
            sensor = "audio",                 # Name of the sensor component
            desc   = "Audio feature",         # Description of the sensor
            base   = "/run/user/{}/audio/{}", # Temporary audio file base path
            paths  = ["source", "normalize", "features"], # Temporary sub-folders
            # seq_cmd  = "auditok -o"         # Audio sequencer program
            seq_cmd  = "./audiseq.py -d 2 -o",# Audio sequencer program
            seq_file = "part_{N}_{start}_{end}.wav", # Audio sequencer output file
            # recognition  = "Speaker_models",
            recog_cfg  = "Speaker_models",    # User recognition configuration folder
            # recog_in   = "normalize",         # User recognition input
            recog_in   = "source",            # User recognition input
            openSmile_cfg = "/usr/local/share/opensmile-2.3.0/config/gemaps/eGeMAPSv01a.conf",
            # openSmile_in  = "normalize",      # openSmile input
            openSmile_in  = "source",         # openSmile input
        ),
        quantity = dict(   
            # data_id      sensor_ids
            snd_source   = 'source',
            f0_semitone  = {'m':  'F0semitoneFrom27.5Hz_sma3nz_amean',            'sd': 'F0semitoneFrom27.5Hz_sma3nz_stddevNorm' },
            f0_semi_per  = {'20': 'F0semitoneFrom27.5Hz_sma3nz_percentile20.0',   '50': 'F0semitoneFrom27.5Hz_sma3nz_percentile50.0', '     80': 'F0semitoneFrom27.5Hz_sma3nz_percentile80.0', 'rng': 'F0semitoneFrom27.5Hz_sma3nz_pctlrange0-2' },
            f0_semi_rise = {'m':  'F0semitoneFrom27.5Hz_sma3nz_meanRisingSlope',  'sd': 'F0semitoneFrom27.5Hz_sma3nz_stddevRisingSlope' },
            f0_semi_fall = {'m':  'F0semitoneFrom27.5Hz_sma3nz_meanFallingSlope', 'sd': 'F0semitoneFrom27.5Hz_sma3nz_stddevFallingSlope' },

            loudness      = {'m':  'loudness_sma3_amean',            'sd': 'loudness_sma3_stddevNorm' },
            loudness_per  = {'20': 'loudness_sma3_percentile20.0',   '50': 'loudness_sma3_percentile50.0', '     80': 'loudness_sma3_percentile80.0', 'rng': 'loudness_sma3_pctlrange0-2' },
            loudness_rise = {'m':  'loudness_sma3_meanRisingSlope',  'sd': 'loudness_sma3_stddevRisingSlope' },
            loudness_fall = {'m':  'loudness_sma3_meanFallingSlope', 'sd': 'loudness_sma3_stddevFallingSlope' },

            spectral = { 'm': 'spectralFlux_sma3_amean',     'sd': 'spectralFlux_sma3_stddevNorm' },
            mfcc1    = { 'm': 'mfcc1_sma3_amean',            'sd': 'mfcc1_sma3_stddevNorm' },
            mfcc2    = { 'm': 'mfcc2_sma3_amean',            'sd': 'mfcc2_sma3_stddevNorm' },
            mfcc3    = { 'm': 'mfcc3_sma3_amean',            'sd': 'mfcc3_sma3_stddevNorm' },
            mfcc4    = { 'm': 'mfcc4_sma3_amean',            'sd': 'mfcc4_sma3_stddevNorm' },
            jitter   = { 'm': 'jitterLocal_sma3nz_amean',    'sd': 'jitterLocal_sma3nz_stddevNorm' },
            shimmer  = { 'm': 'shimmerLocaldB_sma3nz_amean', 'sd': 'shimmerLocaldB_sma3nz_stddevNorm' },
            HNRdBACF = { 'm': 'HNRdBACF_sma3nz_amean',       'sd': 'HNRdBACF_sma3nz_stddevNorm' },
            log_H1H2 = { 'm': 'logRelF0-H1-H2_sma3nz_amean', 'sd': 'logRelF0-H1-H2_sma3nz_stddevNorm' },
            log_H1A3 = { 'm': 'logRelF0-H1-A3_sma3nz_amean', 'sd': 'logRelF0-H1-A3_sma3nz_stddevNorm' },

            f1_freq  = { 'm': 'F1frequency_sma3nz_amean',         'sd': 'F1frequency_sma3nz_stddevNorm' },
            f1_band  = { 'm': 'F1bandwidth_sma3nz_amean',         'sd': 'F1bandwidth_sma3nz_stddevNorm' },
            f1_amp   = { 'm': 'F1amplitudeLogRelF0_sma3nz_amean', 'sd': 'F1amplitudeLogRelF0_sma3nz_stddevNorm' },

            f2_freq  = { 'm': 'F2frequency_sma3nz_amean',         'sd': 'F2frequency_sma3nz_stddevNorm' },
            f2_band  = { 'm': 'F2bandwidth_sma3nz_amean',         'sd': 'F2bandwidth_sma3nz_stddevNorm' },
            f2_amp   = { 'm': 'F2amplitudeLogRelF0_sma3nz_amean', 'sd': 'F2amplitudeLogRelF0_sma3nz_stddevNorm' },

            f3_freq  = { 'm': 'F3frequency_sma3nz_amean',         'sd': 'F3frequency_sma3nz_stddevNorm' },
            f3_band  = { 'm': 'F3bandwidth_sma3nz_amean',         'sd': 'F3bandwidth_sma3nz_stddevNorm' },
            f3_amp   = { 'm': 'F3amplitudeLogRelF0_sma3nz_amean', 'sd': 'F3amplitudeLogRelF0_sma3nz_stddevNorm' },

            alpha_V  = { 'm': 'alphaRatioV_sma3nz_amean',         'sd': 'alphaRatioV_sma3nz_stddevNorm' },
            hamma_V  = { 'm': 'hammarbergIndexV_sma3nz_amean',    'sd': 'hammarbergIndexV_sma3nz_stddevNorm' },
            slopeV0  = { 'm': 'slopeV0-500_sma3nz_amean',         'sd': 'slopeV0-500_sma3nz_stddevNorm' },
            slopeV500= { 'm': 'slopeV500-1500_sma3nz_amean',      'sd': 'slopeV500-1500_sma3nz_stddevNorm' },
            specFluxV= { 'm': 'spectralFluxV_sma3nz_amean',       'sd': 'spectralFluxV_sma3nz_stddevNorm' },
            mfcc1V   = { 'm': 'mfcc1V_sma3nz_amean',              'sd': 'mfcc1V_sma3nz_stddevNorm' },
            mfcc2V   = { 'm': 'mfcc2V_sma3nz_amean',              'sd': 'mfcc2V_sma3nz_stddevNorm' },
            mfcc3V   = { 'm': 'mfcc3V_sma3nz_amean',              'sd': 'mfcc3V_sma3nz_stddevNorm' },
            mfcc4V   = { 'm': 'mfcc4V_sma3nz_amean',              'sd': 'mfcc4V_sma3nz_stddevNorm' },

            alpha_UV   = 'alphaRatioUV_sma3nz_amean',
            hamma_UV   = 'hammarbergIndexUV_sma3nz_amean',
            slopeUV0   = 'slopeUV0-500_sma3nz_amean',
            slopeUV500 = 'slopeUV500-1500_sma3nz_amean',
            specFluxUV = 'spectralFluxUV_sma3nz_amean',

            laudness_pps = 'loudnessPeaksPerSec',
            v_sgm      = 'VoicedSegmentsPerSec',
            v_sgm_len  = { 'm': 'MeanVoicedSegmentLengthSec',     'sd': 'StddevVoicedSegmentLengthSec' },
            uv_sgm_len = { 'm': 'MeanUnvoicedSegmentLength',      'sd': 'StddevUnvoicedSegmentLength' },
            snd_level  = 'equivalentSoundLevel_dBp',
            snd_class  = 'class',

        ),
    ),
)
sensors['AUDIO'] = None

report = dict(    #  source_id            period  precision transport
                  #     [0]                 [1]        [2]       [3]
    humidity      = ('sens_amb_1_humidity',    60,        0,     'mqtt'),
    temperature   = ('sens_amb_1_temp',        60,        1,     'mqtt'),
    temp_raw      = ('sens_amb_1_temp_raw',    60,        1,     'mqtt'),
    # temp_avg    = ('sens_amb_1_temp_avg',   300,        1,     ['mqtt', 'mqtt_dev']),
    pressure      = ('sens_amb_1_press',       60,       -2,     'mqtt'),
    # altitude  = ('sens_amb_1_altitude',    60,        0,     'mqtt_dev'),
    angle         = ('sens_amb_1_angle',       60,        2,     'mqtt'),
    accel         = ('sens_amb_1_accel',       60,        3,     'mqtt'),
    # gyro      = ('sens_amb_1_gyro',        60,        3,     'mqtt_dev'),
    # mag       = ('sens_amb_1_mag',         60,        3,     'mqtt_dev'),
    # uv_index  = ('sens_amb_1_uv_index',    60,        0,     'mqtt_dev'),
    # uv_risk   = ('sens_amb_1_uv_risk',     60,        0,     'mqtt_dev'),
    sens_bed      = ('sens_bed_accel',         10,        3,     'mqtt'),
    sens_belt     = ('sens_belt_accel',        10,        3,     'mqtt'),
    sens_bracelet_left = 
                    ('sens_bracelet_left_accel',  10,        3,     'mqtt'),
    sens_bracelet_right = 
                    ('sens_bracelet_right_accel', 10,        3,     'mqtt'),
    voice_cmd     = ('command',                0,         0,     'mqtt'),

    # Audio features
    # audio_name   = ('name', 120, None, 'mqtt'),
    snd_source    = ('feat_audio_source', 120, None, 'mqtt'),
    f0_semitone   = ('feat_audio_f0_semitone', 120, {'m':1, 'sd':3}, 'mqtt'),
    f0_semi_per   = ('feat_audio_f0_semitone_percentile', 120, {'20':1, '50':1, '80':1, 'rng':2}, 'mqtt'),
    f0_semi_rise  = ('feat_audio_f0_semitone_rising_slope', 120, {'m':0, 'sd':0}, 'mqtt'),
    f0_semi_fall  = ('feat_audio_f0_semitone_falling_slope', 120, {'m':0, 'sd':0}, 'mqtt'),
    
    loudness      = ('feat_audio_loudness', 120, {'m':2, 'sd':3}, 'mqtt'),
    loudness_per  = ('feat_audio_loudness_percentile', 120, {'20':2, '50':2, '80':2, 'rng':3}, 'mqtt'),
    loudness_rise = ('feat_audio_loudness_rising_slope', 120, {'m':2, 'sd':2}, 'mqtt'),
    loudness_fall = ('feat_audio_loudness_falling_slope', 120, {'m':2, 'sd':2}, 'mqtt'),
    
    spectral      = ('feat_audio_spectral_flux', 120, {'m':2, 'sd':3}, 'mqtt'),
    mfcc1         = ('feat_audio_mfcc1', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc2         = ('feat_audio_mfcc2', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc3         = ('feat_audio_mfcc3', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc4         = ('feat_audio_mfcc4', 120, {'m':1, 'sd':3}, 'mqtt'),
    jitter        = ('feat_audio_jitter', 120, {'m':3, 'sd':3}, 'mqtt'),
    shimmer       = ('feat_audio_shimmer', 120, {'m':2, 'sd':3}, 'mqtt'),
    HNRdBACF      = ('feat_audio_harmonics_to_noise_ratio', 120, {'m':2, 'sd':3}, 'mqtt'),
    log_H1H2      = ('feat_audio_harmonic_difference_f0_h1_h2', 120, {'m':2, 'sd':3}, 'mqtt'),
    log_H1A3      = ('feat_audio_harmonic_difference_f0_h1_a3', 120, {'m':1, 'sd':3}, 'mqtt'),
    
    f1_freq       = ('feat_audio_f1_frequency', 120, {'m':0, 'sd':2}, 'mqtt'),
    f1_band       = ('feat_audio_f1_bandwidth', 120, {'m':0, 'sd':2}, 'mqtt'),
    f1_amp        = ('feat_audio_f1_amplitude', 120, {'m':0, 'sd':2}, 'mqtt'),

    f2_freq       = ('feat_audio_f2_frequency', 120, {'m':0, 'sd':2}, 'mqtt'),
    f2_band       = ('feat_audio_f2_bandwidth', 120, {'m':0, 'sd':2}, 'mqtt'),
    f2_amp        = ('feat_audio_f2_amplitude', 120, {'m':0, 'sd':2}, 'mqtt'),

    f3_freq       = ('feat_audio_f3_frequency', 120, {'m':0, 'sd':2}, 'mqtt'),
    f3_band       = ('feat_audio_f3_bandwidth', 120, {'m':0, 'sd':2}, 'mqtt'),
    f3_amp        = ('feat_audio_f3_amplitude', 120, {'m':0, 'sd':2}, 'mqtt'),

    alpha_V       = ('feat_audio_alpha_ratio_voiced', 120, {'m':2, 'sd':3}, 'mqtt'),
    hamma_V       = ('feat_audio_hammarberg_index_voiced', 120, {'m':1, 'sd':3}, 'mqtt'),
    slopeV0       = ('feat_audio_slope_voiced_0-500', 120, {'m':3, 'sd':3}, 'mqtt'),
    slopeV500     = ('feat_audio_slope_voiced_500-1500', 120, {'m':3, 'sd':3}, 'mqtt'),
    specFluxV     = ('feat_audio_spectral_flux_voiced', 120, {'m':2, 'sd':3}, 'mqtt'),
    mfcc1V        = ('feat_audio_mfcc1_voiced', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc2V        = ('feat_audio_mfcc2_voiced', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc3V        = ('feat_audio_mfcc3_voiced', 120, {'m':1, 'sd':3}, 'mqtt'),
    mfcc4V        = ('feat_audio_mfcc4_voiced', 120, {'m':1, 'sd':3}, 'mqtt'),
    
    alpha_UV      = ('feat_audio_alpha_ratio_unvoiced', 120, 2, 'mqtt'),
    hamma_UV      = ('feat_audio_hammarberg_index_unvoiced', 120, 2, 'mqtt'),
    slopeUV0      = ('feat_audio_slope_unvoiced_0-500', 120, 3, 'mqtt'),
    slopeUV500    = ('feat_audio_slope_unvoiced_500-1500', 120, 3, 'mqtt'),
    specFluxUV    = ('feat_audio_spectral_flux_unvoiced', 120, 2, 'mqtt'),

    laudness_pps  = ('feat_audio_loudness_peaks_per_sec', 120, 2, 'mqtt'),
    v_sgm         = ('feat_audio_voiced_segments_per_sec', 120, 2, 'mqtt'),
    v_sgm_len     = ('feat_audio_voiced_segment_length', 120, {'m':3, 'sd':3}, 'mqtt'),
    uv_sgm_len    = ('feat_audio_unvoiced_segment_length', 120, {'m':2, 'sd':2}, 'mqtt'),
    snd_level     = ('feat_audio_equivalent_sound_level', 120, 0, 'mqtt'),
    # snd_class     = ('feat_audio_class', 120, None, 'mqtt'),
)

channels = dict(  # channel parameter              transport/broker
    render  =  ('saam/messages',                   'mqtt'),
    chnl1   = [(['root1/topic1', 'root2/topic2'], ['mqtt1', 'mqtt2']),
               ( 'root3/topic3',                   'mqtt3'          )],
    chnl2   =  (['root4/topic4', 'root5/topic5'],  'mqtt4'          ),
)

render = dict( # Rendering settings
    channel = "render",                # Coaching action input channel
    enabled = True,                    # Coaching is enabled
    periods = [ "9AM", "3PM", "6PM" ], # Rendering time
    history = 24 * 60 * 60,            # Render only events from last day
    delay   = 2    # Delay between consequitive cauching action rendering
)
    
transmit = dict(
    mqtt_dev = dict(
        TYPE      = 'MQTT',
        TOPIC     = 'saam/data',
        HOST      = "... development MQTT host ...",
        PORT      = 8883,
        KEEPALIVE = 60,
        USER      = "username",
        PASS      = "***secret***",
        SSL       = dict(
            CA      = "... Path to CA Certificate ...",
            CERT    = "... Path to authentication certificate ...",
            KEY     = "... Path to authentication secret key ..."
        )
    ),
    mqtt = dict(
        TYPE      = 'MQTT',
        TOPIC     = 'saam/data',
        HOST      = "... MQTT host ...",
        PORT      = 8883,
        KEEPALIVE = 60,
        USER      = "username",
        PASS      = "***secret***",
        SSL       = dict(
            CA      = "... Path to CA Certificate ...",
            CERT    = "... Path to authentication certificate ...",
            KEY     = "... Path to authentication secret key ..."
        )
    )
)


