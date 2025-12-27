import React, { useEffect, useRef } from 'react';

type GradientBackgroundProps = {
  className?: string;
};

export default function GradientBackground({ className = '' }: GradientBackgroundProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animationRef = useRef<number | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const setCanvasSize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
    };

    setCanvasSize();
    window.addEventListener('resize', setCanvasSize);

    const colors = [
      { r: 73, g: 200, b: 103 },   // #49C867
      { r: 52, g: 168, b: 83 },    // #34A853
      { r: 19, g: 177, b: 236 },   // #13B1EC
      { r: 15, g: 30, b: 60 }      // Deep navy
    ];

    let time = 0;

    const drawGradient = () => {
      // Create a subtle animated gradient mesh
      const width = canvas.width;
      const height = canvas.height;

      // Base dark background
      const gradient = ctx.createLinearGradient(0, 0, width, height);
      gradient.addColorStop(0, '#0a0e27');
      gradient.addColorStop(1, '#0f1f3c');
      ctx.fillStyle = gradient;
      ctx.fillRect(0, 0, width, height);

      // Animated gradient orbs
      const orbs = [
        { x: width * 0.2, y: height * 0.3, color: colors[0], size: 400 },
        { x: width * 0.8, y: height * 0.2, color: colors[2], size: 350 },
        { x: width * 0.5, y: height * 0.8, color: colors[1], size: 380 }
      ];

      orbs.forEach((orb, i) => {
        // Subtle floating animation
        const offsetX = Math.sin(time * 0.0003 + i) * 50;
        const offsetY = Math.cos(time * 0.0002 + i) * 50;

        const radialGrad = ctx.createRadialGradient(
          orb.x + offsetX,
          orb.y + offsetY,
          0,
          orb.x + offsetX,
          orb.y + offsetY,
          orb.size
        );

        radialGrad.addColorStop(0, `rgba(${orb.color.r}, ${orb.color.g}, ${orb.color.b}, 0.15)`);
        radialGrad.addColorStop(0.5, `rgba(${orb.color.r}, ${orb.color.g}, ${orb.color.b}, 0.05)`);
        radialGrad.addColorStop(1, `rgba(${orb.color.r}, ${orb.color.g}, ${orb.color.b}, 0)`);

        ctx.fillStyle = radialGrad;
        ctx.fillRect(0, 0, width, height);
      });

      time++;
    };

    const animate = () => {
      drawGradient();
      animationRef.current = requestAnimationFrame(animate);
    };

    animate();

    return () => {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
      window.removeEventListener('resize', setCanvasSize);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      className={`fixed inset-0 w-full h-full z-0 ${className}`}
      style={{ display: 'block' }}
    />
  );
}
