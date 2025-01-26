#mqtt-to-mongo.js
const mqtt = require('mqtt');
const { MongoClient } = require('mongodb');

// MQTT broker details
const mqttBroker = 'mqtt://127.0.0.1'; // Replace with your broker's IP if needed
const mqttTopic = 'garbage/#'; // Wildcard to subscribe to all garbage topics

// MongoDB connection details
const mongoURI = 'mongodb://127.0.0.1:27017'; // MongoDB server
const dbName = 'garbage_data'; // Database name
const collectionName = 'sensor_data'; // Collection name

// Connect to MongoDB
const mongoClient = new MongoClient(mongoURI); // Removed useUnifiedTopology
mongoClient.connect().then(() => {
    console.log('Connected to MongoDB');
    const db = mongoClient.db(dbName);
    const collection = db.collection(collectionName);

    // Connect to MQTT broker
    const mqttClient = mqtt.connect(mqttBroker);
    mqttClient.on('connect', () => {
        console.log('Connected to MQTT broker');
        mqttClient.subscribe(mqttTopic, (err) => {
            if (err) console.error('Failed to subscribe:', err);
        });
    });

    // Handle incoming MQTT messages
    mqttClient.on('message', (topic, message) => {
        console.log(`Received message: ${message.toString()} on topic: ${topic}`);

        // Parse JSON payload from MQTT message
        let parsedData;
        try {
            parsedData = JSON.parse(message.toString());
        } catch (error) {
            console.error('Failed to parse message:', error);
            return;
        }

        // Extract necessary data, including waste level
        const { distance, motion, lidStatus, wasteLevel, timestamp } = parsedData;
        
        // If waste level exists in the message, include it
        const data = {
            topic: topic,
            distance: distance || null,
            motion: motion || null,
            lidStatus: lidStatus || null,
            wasteLevel: wasteLevel || null, // Include waste level
            timestamp: timestamp || new Date(),
        };

        // Insert data into MongoDB
        collection.insertOne(data).then(() => {
            console.log('Data inserted into MongoDB');
        }).catch(err => {
            console.error('Failed to insert data:', err);
        });
    });
}).catch(err => {
    console.error('Failed to connect to MongoDB:', err);
});
