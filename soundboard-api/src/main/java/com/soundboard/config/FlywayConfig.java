// /* FlywayConfig removed — migrations will be managed by
//    `spring-boot-starter-flyway` (dependency declared in pom.xml). */
// package com.soundboard.config;

// import org.flywaydb.core.Flyway;
// import org.slf4j.Logger;
// import org.slf4j.LoggerFactory;
// import org.springframework.boot.ApplicationArguments;
// import org.springframework.boot.ApplicationRunner;
// import org.springframework.context.annotation.Bean;
// import org.springframework.context.annotation.Configuration;

// import javax.sql.DataSource;

// @Configuration
// public class FlywayConfig {

//     private static final Logger log = LoggerFactory.getLogger(FlywayConfig.class);

//     @Bean
//     public ApplicationRunner runMigrations(DataSource dataSource) {
//         return new ApplicationRunner() {
//             @Override
//             public void run(ApplicationArguments args) throws Exception {
//                 log.info("Running programmatic Flyway migrations (flyway-core)");
//                 Flyway flyway = Flyway.configure()
//                         .dataSource(dataSource)
//                         .locations("classpath:db/migration")
//                         .baselineOnMigrate(true)
//                         .load();
//                 flyway.migrate();
//                 log.info("Flyway migrations finished");
//             }
//         };
//     }
// }
package com.soundboard.config;

public final class FlywayConfig {
	private FlywayConfig() {}
}

