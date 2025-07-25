# Utilities used by the CS Plotter project.
#
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

from pyqtgraph.Qt import QtWidgets, QtCore

Y_LIM_M = None # If None, the scale will be dynamic
X_SCALE_S = 30 
X_PADDING_S = 1 # Seconds of padding added to the x-axis
PLOT_REFRESH_PERIOD_MS = 30
BUFFER_SIZE = int(X_SCALE_S / (PLOT_REFRESH_PERIOD_MS / 1000))  # Number of samples displayed
LABEL_TEXT_SIZE_PX = 150
WINDOW_SIZE = (1440, 1080)

DATATYPE_CS_DISTANCE = 0
DATATYPE_RSSI_DISTANCE = 1
DATATYPE_VELOCITY = 2

SUBSTRING_CS_DISTANCE = "main mode result"
SUBSTRING_RSSI_DISTANCE = "RSSI"
SUBSTRING_VELOCITY = "Velocity"

def parse_data(data):
    if SUBSTRING_CS_DISTANCE in data:
        data_parsed = int(data.split(' ')[-2])
        data_type = DATATYPE_CS_DISTANCE
    elif SUBSTRING_RSSI_DISTANCE in data:
        data_parsed = int(data.split(' ')[-2])
        data_type = DATATYPE_RSSI_DISTANCE
    elif SUBSTRING_VELOCITY in data:
        data_parsed = float(data.split(' ')[-1].strip("\n"))
        data_type = DATATYPE_VELOCITY    
    else:
        data_parsed = None
        data_type = None
    
    return data_parsed, data_type

def create_label_widget():
    frame  = QtWidgets.QFrame()
    layout = QtWidgets.QVBoxLayout()
    label = QtWidgets.QLabel()
    layout.addWidget(label)
    layout.setAlignment(label, QtCore.Qt.AlignCenter)
    frame.setLayout(layout)
    
    return frame, label
        
def mm_to_m(distance_mm):
    return round(distance_mm / 1000.0, 1)