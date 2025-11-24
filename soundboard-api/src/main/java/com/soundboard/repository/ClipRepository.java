package com.soundboard.repository;

import com.soundboard.model.Clip;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface ClipRepository extends JpaRepository<Clip, Long> {
    
    Page<Clip> findAllByOrderByUploadDateDesc(Pageable pageable);
    
    List<Clip> findByTitleContainingIgnoreCase(String title);
    
    List<Clip> findByUploadedBy(String uploadedBy);
    
    List<Clip> findByIsProcessed(Boolean isProcessed);
    
    @Query("SELECT c FROM Clip c WHERE " +
           "LOWER(c.title) LIKE LOWER(CONCAT('%', :query, '%')) OR " +
           "LOWER(c.description) LIKE LOWER(CONCAT('%', :query, '%'))")
    Page<Clip> searchClips(@Param("query") String query, Pageable pageable);
}