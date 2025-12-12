package com.soundboard.repository;

import com.soundboard.model.PlayStats;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface PlayStatsRepository extends JpaRepository<PlayStats, Long> {

    Optional<PlayStats> findByClipId(Long clipId);

    @Query("SELECT ps FROM PlayStats ps ORDER BY ps.playCount DESC")
    Page<PlayStats> findAllOrderByPlayCountDesc(Pageable pageable);
}
