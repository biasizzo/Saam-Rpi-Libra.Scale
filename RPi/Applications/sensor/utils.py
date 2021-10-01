import zmq
import time
import logging
# 2019-12-21
import psutil, socket

from zmq.eventloop import ioloop, zmqstream
from setproctitle import setproctitle

"""
This module contains some utility functions to help when using the lower level
zmq messaging protocol malOS exposes.  There are three functions below, one to
register a data callback, one to register an error callback, and another to
register a keep alive.  The main difference between these functions is the port
I conntect to since there are tcp ports for errors, data, and keep alive.
"""

stop_msg = "{} callback @ {} has stopped processing messages."

def register_data_callback(callback, name, creator_ip, sensor_port):
    
    """Accepts a function to run when malOS zqm driver pushes an update"""

    setproctitle(name+'_data')
    # Grab a zmq context, as per usual, connect to it, but make it a SUBSCRIPTION this time
    context = zmq.Context()
    socket = context.socket(zmq.SUB)

    # Connect to the data socket (base sensor port + 3)
    socket.connect('tcp://{0}:{1}'.format(creator_ip, sensor_port + 3))

    # Set socket options to subscribe and send off en empty string to let it know we're ready
    socket.setsockopt(zmq.SUBSCRIBE, b'')

    # Create the stream to listen to
    stream = zmqstream.ZMQStream(socket)

    # When data comes across the stream, execute the callback with it's contents as parameters
    stream.on_recv(callback)

    # Print some debug information
    #print('Connected to data publisher with port {0}'.format(data_port))

    # Start a global IO loop from tornado
    try:
        ioloop.IOLoop.instance().start()
    except (KeyboardInterrupt, SystemExit):
        ioloop.IOLoop.instance().stop()
    # print('Worker has stopped processing messages.')
    logging.warning(stop_msg.format('Data', sensor_port))


def register_error_callback(callback, name, creator_ip, sensor_port):
    """Accepts a function to run when the malOS zqm driver pushes an error"""

    setproctitle(name+'_error')
    # Grab a zmq context, as per usual, connect to it, but make it a SUBSCRIPTION this time
    context = zmq.Context()
    socket = context.socket(zmq.SUB)

    # Connect to the error socket (sensor_port + 2)
    socket.connect('tcp://{0}:{1}'.format(creator_ip, sensor_port + 2))

    # Set socket options to subscribe and send off en empty string to let it know we're ready
    socket.setsockopt(zmq.SUBSCRIBE, b'')

    # Create a stream to listen to
    stream = zmqstream.ZMQStream(socket)

    # When data comes across the stream, execute the callback with it's contents as parameters
    stream.on_recv(callback)

    # Print some debug information
    # print('Connected to error publisher with port {0}'.format(error_port))

    #Start a global IO loop from tornado
    try:
        ioloop.IOLoop.instance().start()
    except (KeyboardInterrupt, SystemExit):
        ioloop.IOLoop.instance().stop()
    # print('Worker has stopped processing messages.')
    logging.warning(stop_msg.format('Error', sensor_port))


def driver_keep_alive(name, creator_ip, sensor_port, ping=5):
    """
    This doesn't take a callback function as it's purpose is very specific.
    This will ping the driver every n seconds to keep the driver alive and sending updates
    """

    # Close inherited socket connection 
    for connection in psutil.Process().connections():
       socket.socket(fileno=connection.fd).close()
       
    setproctitle(name+'_keep-alive')
    # Grab zmq context
    context = zmq.Context()

    # Set up socket as a push
    sping = context.socket(zmq.PUSH)

    # Connect to the keep-alive socket (sensor_port + 1)
    sping.connect('tcp://{0}:{1}'.format(creator_ip, sensor_port + 1))

    # Start a forever loop
    try:
        while True:
            # Ping with empty string to let the drive know we're still listening
            sping.send_string('')
       
            # Delay between next ping
            time.sleep(ping)
    except KeyboardInterrupt:
        logging.warning(stop_msg.format('Keep-Alive', sensor_port))
