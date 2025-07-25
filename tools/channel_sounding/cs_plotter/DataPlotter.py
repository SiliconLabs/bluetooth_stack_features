# Python class for plotting data in real time.
# 
# This file is part of the CS Plotter project.
# CS Plotter is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# CS Plotter is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should find a copy of the GNU General Public License
# in the project repository. If not, see <https://www.gnu.org/licenses/>.

import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, QtCore
import signal
import collections
import time
from utils import *

class DataPlotter(QtWidgets.QMainWindow):
    def __init__(self, window_size: tuple, 
                 refresh_period: int,
                 buffer_size: int,
                 data_queue):
        self._window_size = window_size
        self._refresh_period = refresh_period
        
        self._plot = None
        self._cs_filtered_label = None
        self._rssi_label = None
        self._velocity_label = None
        self._update_rate_label = None
        
        self._cs_data_buffer = collections.deque([0], maxlen=buffer_size)
        self._rssi_data_buffer = collections.deque([0], maxlen=buffer_size)
        self._velocity_buffer = collections.deque([0], maxlen=buffer_size)
        self._time_buffer = collections.deque([0], maxlen=buffer_size)
        self._data_queue = data_queue
        
        self._timer = QtCore.QTimer()
        self._start_time = 0
        self._previous_sample_time = 0
        
        # Start main app, otherwise widgets can't be added
        self._app = QtWidgets.QApplication.instance()
        if not self._app:  # Create a new instance if it doesn't exist
            self._app = QtWidgets.QApplication([])
        
        super().__init__()
        self._setup_window()
        
        # Add handler for ctrl+c. This way we can stop the timers before closing the app
        signal.signal(signal.SIGINT, self._handle_sigint)  
        
    def _setup_window(self):
        pg.setConfigOption("background", "#F4F4F4")
        pg.setConfigOption("foreground", "#333333")
        
        self.setWindowTitle("CS Plotter")
        self.resize(self._window_size[0], self._window_size[1])
        
        # Create a central widget and set it as the main window's central widget
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        
        # Creat splitters so that we can ditribute the widgets in the main widget 
        upper_upper_splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        upper_lower_splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        main_splitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        

        # Create widgets for the upper portion of the main widget
        cs_filtered_widget, self._cs_filtered_label = create_label_widget()
        rssi_widget, self._rssi_label = create_label_widget()
        velocity_widget, self._velocity_label = create_label_widget()
        update_rate_widget, self._update_rate_label = create_label_widget()
        
        upper_upper_splitter.addWidget(cs_filtered_widget)
        upper_upper_splitter.addWidget(velocity_widget)
        
        plot_window = pg.GraphicsLayoutWidget(title="Real-Time CS Distance Plot")
        self._plot = plot_window.addPlot()
        self._plot.setLabel("left", "Distance (m) and Velocity (m/s)", color="#555555", size="12pt")
        self._plot.setLabel("bottom", "Time (s)", color="#555555", size="12pt")
        self._plot.addLegend(offset=(-1,1))
        
        self._curve_cs_distance = self._plot.plot(pen=pg.mkPen("#3498DB", width=5), name="CS FILTERED")
        self._curve_rssi_distance = self._plot.plot(pen=pg.mkPen("#E74C3C", width=3), name="RSSI")
        self._curve_velocity = self._plot.plot(pen=pg.mkPen("#E9B36C", width=3), name="VELOCITY")
        
        # Add the plot and upper split to the main splitter. Now we should have plot in the lower and values in the upper section
        main_splitter.addWidget(upper_upper_splitter)
        main_splitter.addWidget(upper_lower_splitter)
        main_splitter.addWidget(plot_window)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(main_splitter)
        central_widget.setLayout(layout)
        
        self._rssi_label.setText(
                f"<span style='font-size: 200pt; font-weight: bold; color: #2C3E50;'>"
                f"Distance"
            )
        
    def start_plot(self):
        # Initialize the timer so that we get periodic updates to the plot
        self._start_time = time.perf_counter()
        self._previous_sample_time = self._start_time
        self._timer.timeout.connect(self._update_plot)
        self._timer.start(self._refresh_period)
        
        self.show()
        self._app.exec()
        
    def _update_plot(self):
        cs_distance_received = False
        rssi_distance_received = False
        velocity_received = False
        
        while not self._data_queue.empty():
            try:
                serial_data = self._data_queue.get().decode("utf-8")
                parsed_serial_data, datatype = parse_data(serial_data)
                if datatype == DATATYPE_CS_DISTANCE and cs_distance_received == False:
                    distance_m = mm_to_m(parsed_serial_data)
                    self._cs_data_buffer.append(distance_m)
                    cs_distance_received = True
                elif datatype == DATATYPE_RSSI_DISTANCE and rssi_distance_received == False:
                    distance_m = mm_to_m(parsed_serial_data)
                    self._rssi_data_buffer.append(distance_m)
                    rssi_distance_received = True
                elif datatype == DATATYPE_VELOCITY and velocity_received == False:
                    self._velocity_buffer.append(parsed_serial_data)
                    velocity_received = True
            except UnicodeError:
                print("Failed at decoding serial data! Please ensure the data format complies with the format specified in the README's FAQ.")
                quit()
            except Exception as e:
                print(f"Error processing queue data: {e}")

        current_time = time.perf_counter() - self._start_time
        self._time_buffer.append(current_time)
        
        # Display the values, and if no data was received append the last read value to the buffers
        if not cs_distance_received:
            self._cs_data_buffer.append(self._cs_data_buffer[-1])
            
        self._cs_filtered_label.setText(
            f"<div style='text-align: center;'><span style='font-size: 75px;font-weight:bold; color: #2C3E50;'>Distance</span><br><span style='font-size: {LABEL_TEXT_SIZE_PX}px;font-weight: bold; color: #2C3E50;'>{self._cs_data_buffer[-1]} m</span>"
        )   
            
        if not velocity_received:
            self._velocity_buffer.append(self._velocity_buffer[-1])
        
        self._velocity_label.setText(
            f"<div style='text-align: center;'><span style='font-size: 75px;font-weight:bold; color: #2C3E50;'>Velocity</span><br><span style='font-size: {LABEL_TEXT_SIZE_PX}px;font-weight: bold; color: #2C3E50;'>{self._velocity_buffer[-1]} m/s</span>"""
        )
        
        if not rssi_distance_received:
            self._rssi_data_buffer.append(self._rssi_data_buffer[-1])

        self._curve_cs_distance.setData(self._time_buffer, self._cs_data_buffer)
        self._curve_rssi_distance.setData(self._time_buffer, self._rssi_data_buffer)
        self._curve_velocity.setData(self._time_buffer, self._velocity_buffer)
        
        # Adjust the Y-range
        if Y_LIM_M != None:
            y_lim = Y_LIM_M
        else:
            y_lim = max(max(self._cs_data_buffer), max(self._rssi_data_buffer))
        self._plot.setYRange(-y_lim, y_lim)
        self._plot.setXRange(self._time_buffer[0], self._time_buffer[-1] + X_PADDING_S)
        
        
    def _handle_sigint(self, signum, frame):
        print("\nCtrl+C detected. Closing application...")
        self._timer.stop()
        self._app.quit() 
        