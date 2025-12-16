/* FlywayConfig removed — migrations will be managed by
   `spring-boot-starter-flyway` (dependency declared in pom.xml). */
// package com.soundboard.config;

// import org.flywaydb.core.Flyway;
// import org.slf4j.Logger;
// import org.slf4j.LoggerFactory;
// import org.springframework.beans.factory.annotation.Value;
// import org.springframework.boot.ApplicationArguments;
// import org.springframework.boot.ApplicationRunner;
// import org.springframework.context.annotation.Bean;
// import org.springframework.context.annotation.Configuration;

// import javax.sql.DataSource;

// @Configuration
// public class FlywayConfig {
// 	private static final Logger log = LoggerFactory.getLogger(FlywayConfig.class);

// 	@Value("${app.flyway.repair-on-start:false}")
// 	private boolean repairOnStart;

// 	/**
// 	 * Optional: repair Flyway metadata and run migrate on startup.
// 	 * Controlled by property `app.flyway.repair-on-start` (default: false).
// 	 */
// 	@Bean
// 	public ApplicationRunner repairAndMigrate(DataSource dataSource) {
// 		return new ApplicationRunner() {
// 			@Override
// 			public void run(ApplicationArguments args) throws Exception {
// 				if (!repairOnStart) {
// 					log.info("Flyway repair-on-start disabled. Relying on Spring Boot Flyway auto-migrate.");
// 					return;
// 				}

// 				Flyway flyway = Flyway.configure()
// 						.dataSource(dataSource)
// 						.locations("classpath:db/migration")
// 						.baselineOnMigrate(true)
// 						.load();

// 				try {
// 					log.warn("Running Flyway repair to fix checksum mismatches if present");
// 					flyway.repair();
// 				} catch (Exception e) {
// 					log.error("Flyway repair failed: {}", e.getMessage(), e);
// 					throw e;
// 				}

// 				log.info("Running Flyway migrate");
// 				flyway.migrate();
// 				log.info("Flyway migrations finished");
// 			}
// 		};
// 	}
// }
