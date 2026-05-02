//! Модуль MQTT клиента (rumqttc)

use rumqttc::{AsyncClient, MqttOptions, QoS};
use serde_json::Value;
use tokio::time::{sleep, Duration};

pub struct ClientConfig {
    pub client_id: String,
    pub broker_addr: String,
    pub subscribe_topics: Vec<String>,
    pub keep_alive_secs: u64,
}

impl Default for ClientConfig {
    fn default() -> Self {
        Self {
            client_id: "rust_subscriber".to_string(),
            broker_addr: crate::broker::get_broker_address().to_string(),
            subscribe_topics: vec!["esp32/distance".to_string()],
            keep_alive_secs: 5,
        }
    }
}

pub fn create_mqtt_options(config: &ClientConfig) -> MqttOptions {
    let parts: Vec<&str> = config.broker_addr.split(':').collect();
    let host = parts.get(0).unwrap_or(&"192.168.137.1").to_string();
    let port: u16 = parts.get(1).unwrap_or(&"1883").parse().unwrap_or(1883);
    
    let mut options = MqttOptions::new(&config.client_id, host, port);
    options.set_keep_alive(Duration::from_secs(config.keep_alive_secs));
    options.set_clean_session(true);
    options
}

pub async fn run_client(config: ClientConfig) {
    println!(" Клиент '{}' подключается к {}...", 
        config.client_id, config.broker_addr);
    
    let options = create_mqtt_options(&config);
    let (client, mut eventloop) = AsyncClient::new(options, 10);
    
    for topic in &config.subscribe_topics {
        if let Err(e) = client.subscribe(topic, QoS::AtMostOnce).await {
            eprintln!("Ошибка подписки: {}", e);
            return;
        }
        println!("Подписан на '{}'", topic);
    }
    
    println!("Ожидание сообщений от ESP32...");
    let _ = client.publish("status/rust_client", QoS::AtMostOnce, false, "online").await;
    
    loop {
        match eventloop.poll().await {
            Ok(rumqttc::Event::Incoming(rumqttc::Packet::Publish(p))) => {
                handle_message(&client, &p).await;
            }
            Ok(_) => {}
            Err(e) => {
                eprintln!(" ==Ошибка: {}", e);
                sleep(Duration::from_secs(5)).await;
            }
        }
    }
}

async fn handle_message(_client: &AsyncClient, publish: &rumqttc::Publish) {
    let payload = String::from_utf8_lossy(&publish.payload);
    println!("[{}] {}", publish.topic, payload);
    
    if let Ok(json) = serde_json::from_str::<Value>(&payload) {
        if let Some(dist) = json["distance"].as_f64() {
            println!("|| Расстояние: {:.2} см", dist);
            if dist < 10.0 {
                println!("   ==ТРЕВОГА!");
            }
        }
    }
}