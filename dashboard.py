#dashboard.py
import dash
from dash import dcc, html
from dash.dependencies import Input, Output
import paho.mqtt.client as mqtt
import threading
import json
import time
import dash_bootstrap_components as dbc

# Global variables for data
ultrasonic_data = []
timestamps = []
lid_status = "Closed"
waste_level = "green"  # Initialize waste level

# MQTT setup
MQTT_BROKER = "34.55.234.96"
MQTT_PORT = 1883
TOPIC_TELEMETRY = "garbage/telemetry"

def on_message(client, userdata, msg):
    global ultrasonic_data, timestamps, lid_status, waste_level
    try:
        payload = json.loads(msg.payload.decode())
        distance = payload.get("distance", 0)
        lid_status = payload.get("lidStatus", "Closed")
        waste_level = payload.get("wasteLevel", "green")  # New field for waste level
        timestamp = time.strftime('%H:%M:%S', time.localtime())

        ultrasonic_data.append(distance)
        timestamps.append(timestamp)

        if len(ultrasonic_data) > 20:
            ultrasonic_data = ultrasonic_data[-20:]
            timestamps = timestamps[-20:]

    except Exception as e:
        print(f"Error processing message: {e}")

def mqtt_thread():
    client = mqtt.Client()
    client.username_pw_set("ESP32User", password="cpc357")
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    print("Subscribed to topic:", TOPIC_TELEMETRY)  # Confirm subscription
    client.subscribe(TOPIC_TELEMETRY)
    client.loop_forever()

# Start MQTT client in a separate thread
thread = threading.Thread(target=mqtt_thread)
thread.daemon = True
thread.start()

# Dash App
app = dash.Dash(__name__, external_stylesheets=[dbc.themes.BOOTSTRAP])

app.layout = html.Div(
    style={'backgroundColor': '#f4f6f9', 'padding': '20px'},  # Soft background color for the page
    children=[
        html.H1("Smart Garbage Monitoring System", 
                style={'textAlign': 'center', 'color': '#4a4a4a', 'fontFamily': 'Montserrat', 'fontSize': '36px', 'marginBottom': '40px'}),
        
        dbc.Row([
            dbc.Col(
                dcc.Graph(id="ultrasonic-graph"),
                width=12,
            )
        ], justify="center"),

        html.Div(id="lid-status", style={'textAlign': 'center', 'fontSize': 24, 'color': '#4a4a4a', 'marginTop': 20}),
        
        html.Div(id="waste-level-indicator", 
                 style={'textAlign': 'center', 'fontSize': 24, 'marginTop': 20, 'fontWeight': 'bold'}),
        
        dcc.Interval(
            id='interval-component',
            interval=1000,  # Update every second
            n_intervals=0
        ),
    ]
)

@app.callback(
    [Output('ultrasonic-graph', 'figure'),
     Output('lid-status', 'children'),
     Output('waste-level-indicator', 'children')],
    [Input('interval-component', 'n_intervals')]
)
def update_dashboard(n):
    global ultrasonic_data, timestamps, lid_status, waste_level

    figure = {
        'data': [{
            'x': timestamps,
            'y': ultrasonic_data,
            'type': 'line',
            'name': 'Distance (cm)',
            'line': {'color': '#3498db'},  # Soft blue color for the line
        }],
        'layout': {
            'title': 'Ultrasonic Sensor Over Time',
            'xaxis': {'title': 'Time'},
            'yaxis': {'title': 'Distance (cm)'},
            'height': 400,
            'plot_bgcolor': '#f4f6f9',  # Light background color for the plot area
            'paper_bgcolor': '#f4f6f9',  # Consistent background color for paper
            'font': {'color': '#4a4a4a'},  # Neutral gray text color for labels and title
        }
    }

    lid_status_text = f"Lid Status: {lid_status.upper()}"
    waste_level_text = f"Waste Level: {waste_level.upper()}"

    # Waste level indicator styling
    if waste_level == "green":
        waste_level_color = "#28a745"  # Green
    elif waste_level == "yellow":
        waste_level_color = "#ffc107"  # Yellow
    else:
        waste_level_color = "#dc3545"  # Red

    waste_level_text = f"Waste Level: {waste_level.upper()}"
    
    return figure, lid_status_text, html.Div(waste_level_text, style={'color': waste_level_color})

if __name__ == '__main__':
    app.run_server(debug=True, host='0.0.0.0')
