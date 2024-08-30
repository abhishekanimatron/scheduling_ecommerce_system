# Process Scheduling Algorithm for E-Commerce Platform

## Overview

Design a scheduling algorithm for an e-commerce platform with multiple services, each handling different transaction types. Each service has worker threads with varying priorities and resource limits.

## System Details

- **Services:** Multiple services, each handling specific transactions (e.g., payment, order).
- **Worker Threads:** Each service has threads with priority levels and resources.
  - **Priority Level:** Determines scheduling order.
  - **Resources:** Limits the number of simultaneous requests.
- **Request Handling:** Requests are queued by type and assigned to threads based on priority and resources.
