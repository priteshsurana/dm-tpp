add_library(
    messaging STATIC
    src/kafka_producer.cpp
    src/kafka_consumer.cpp
    src/offset_manager.cpp
    src/rabbitmq_publisher.cpp
    src/rabbitmq_consumer.cpp
)

target_include_directories(messaging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(messaging
    PUBLIC 
        core
        persistence
        RdKafka::rdkafka++
        amqpcpp::amqpcpp
        httplib::httplib
)