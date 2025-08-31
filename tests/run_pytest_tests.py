#!/usr/bin/env python3
"""
Pytest Test Runner for Enhanced BLE Tests

This script provides a convenient interface for running pytest-based BLE tests
with various options and configurations.

Features:
- Run specific test suites or all tests
- Generate HTML and JSON reports
- Handle test markers and filtering
- Setup validation and environment checks
- Comprehensive reporting and logging
"""

import sys
import subprocess
import argparse
import logging
from pathlib import Path

logger = logging.getLogger(__name__)

class PytestRunner:
    """Enhanced pytest runner with BLE-specific configurations"""
    
    def __init__(self):
        self.base_args = [
            sys.executable, "-m", "pytest",
            "-v",  # Verbose output
            "--tb=short",  # Short traceback format
            "--strict-markers",  # Strict marker validation
            "--strict-config",  # Strict configuration validation
        ]
    
    def validate_environment(self):
        """Validate test environment and requirements"""
        issues = []
        
        # Check if pytest is installed
        try:
            import pytest
            logger.info(f"✅ pytest {pytest.__version__} found")
        except ImportError:
            issues.append("pytest not installed")
        
        # Check if required modules are available
        required_modules = ['bleak', 'serial', 'asyncio']
        for module in required_modules:
            try:
                __import__(module)
                logger.info(f"✅ {module} module available")
            except ImportError:
                issues.append(f"{module} module not available")
        
        # Check if requirements file exists
        if not Path("../requirements.txt").exists():
            issues.append("requirements.txt not found in project root")
        
        # Check if test files exist
        test_files = [
            'test_wasm_service.py',
            'test_ble_comprehensive.py',
            'test_sprite_service.py',
            'conftest.py'
        ]
        
        for test_file in test_files:
            if Path(test_file).exists():
                logger.info(f"✅ {test_file} found")
            else:
                issues.append(f"{test_file} not found")
        
        # Check serial port (optional warning)
        serial_port = "/dev/tty.usbmodem0010500306563"
        if not Path(serial_port).exists():
            logger.warning(f"⚠️ Serial port {serial_port} not found - tests may skip serial monitoring")
        else:
            logger.info(f"✅ Serial port {serial_port} available")
        
        if issues:
            logger.error("❌ Environment validation failed:")
            for issue in issues:
                logger.error(f"  - {issue}")
            logger.error("")
            logger.error("💡 To fix missing dependencies:")
            logger.error("  cd .. && pip install -r requirements.txt")
            return False
        
        logger.info("✅ Environment validation passed")
        return True
    
    def run_tests(self, test_suite=None, markers=None, output_dir="test_results", 
                  generate_reports=True, parallel=False, coverage=False):
        """Run pytest tests with specified options"""
        
        args = self.base_args.copy()
        
        # Add test selection
        if test_suite:
            if test_suite == "wasm":
                args.append("test_wasm_service.py")
            elif test_suite == "ble" or test_suite == "comprehensive":
                args.append("test_ble_comprehensive.py")
            elif test_suite == "sprite":
                args.append("test_sprite_service.py")
            else:
                logger.error(f"Unknown test suite: {test_suite}")
                return False
        
        # Add marker filtering
        if markers:
            for marker in markers:
                args.extend(["-m", marker])
        
        # Add parallel execution
        if parallel:
            args.extend(["-n", "auto"])  # Requires pytest-xdist
        
        # Add coverage
        if coverage:
            args.extend(["--cov=.", "--cov-report=html", f"--cov-report=html:{output_dir}/coverage"])
        
        # Add report generation
        if generate_reports:
            Path(output_dir).mkdir(exist_ok=True)
            args.extend([
                f"--html={output_dir}/report.html",
                "--self-contained-html",
                f"--json-report={output_dir}/report.json"
            ])
        
        # Add timeout
        args.extend(["--timeout=300"])  # 5 minute timeout per test
        
        logger.info("🚀 Running pytest with arguments:")
        logger.info(f"   {' '.join(args)}")
        
        try:
            result = subprocess.run(args, check=False)
            return result.returncode == 0
        except Exception as e:
            logger.error(f"❌ Failed to run pytest: {e}")
            return False

def main():
    """Main entry point with command line argument parsing"""
    parser = argparse.ArgumentParser(
        description='Run pytest-based BLE tests with enhanced features',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Test Suites:
  wasm         - WASM service tests (upload, execution, status)
  ble          - Comprehensive BLE tests (device info, control, data, DFU)
  sprite       - Sprite service tests (upload, download, verification)
  
Markers:
  @pytest.mark.wasm        - WASM service specific tests
  @pytest.mark.sprite      - Sprite service specific tests
  @pytest.mark.ble         - General BLE tests
  @pytest.mark.integration - Integration tests
  @pytest.mark.slow        - Slow running tests

Examples:
  python3 run_pytest_tests.py                          # Run all tests
  python3 run_pytest_tests.py --suite wasm             # Run only WASM tests
  python3 run_pytest_tests.py --markers "not slow"     # Skip slow tests
  python3 run_pytest_tests.py --markers "wasm and integration"  # Specific markers
  python3 run_pytest_tests.py --no-reports             # Skip HTML/JSON reports
  python3 run_pytest_tests.py --parallel               # Run tests in parallel
  python3 run_pytest_tests.py --coverage               # Generate coverage report
        """
    )
    
    parser.add_argument(
        '--suite', '-s',
        choices=['wasm', 'ble', 'comprehensive', 'sprite'],
        help='Test suite to run (default: all)'
    )
    
    parser.add_argument(
        '--markers', '-m',
        nargs='+',
        help='Pytest markers to filter tests (e.g., "not slow", "wasm")'
    )
    
    parser.add_argument(
        '--output-dir', '-o',
        default='test_results',
        help='Output directory for reports (default: test_results)'
    )
    
    parser.add_argument(
        '--no-reports',
        action='store_true',
        help='Skip generating HTML and JSON reports'
    )
    
    parser.add_argument(
        '--parallel', '-p',
        action='store_true',
        help='Run tests in parallel (requires pytest-xdist)'
    )
    
    parser.add_argument(
        '--coverage', '-c',
        action='store_true',
        help='Generate code coverage report'
    )
    
    parser.add_argument(
        '--validate-only',
        action='store_true',
        help='Only validate environment, do not run tests'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose logging'
    )
    
    args = parser.parse_args()
    
    # Configure logging
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=log_level,
        format='%(asctime)s [%(levelname)8s] %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    
    runner = PytestRunner()
    
    logger.info("🧪 Enhanced BLE Pytest Runner")
    logger.info("="*60)
    
    # Validate environment
    if not runner.validate_environment():
        logger.error("❌ Environment validation failed")
        sys.exit(1)
    
    if args.validate_only:
        logger.info("✅ Environment validation complete")
        sys.exit(0)
    
    # Run tests
    logger.info("🚀 Starting test execution...")
    
    success = runner.run_tests(
        test_suite=args.suite,
        markers=args.markers,
        output_dir=args.output_dir,
        generate_reports=not args.no_reports,
        parallel=args.parallel,
        coverage=args.coverage
    )
    
    logger.info("="*60)
    
    if success:
        logger.info("🎉 All tests completed successfully!")
        if not args.no_reports:
            logger.info(f"📊 Reports generated in: {args.output_dir}/")
        sys.exit(0)
    else:
        logger.error("❌ Some tests failed or encountered errors")
        if not args.no_reports:
            logger.info(f"📊 Check reports in: {args.output_dir}/")
        sys.exit(1)

if __name__ == "__main__":
    main()
